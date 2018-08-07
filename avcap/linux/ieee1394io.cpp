/*
* ieee1394io.cc -- asynchronously grabbing DV data
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/**
    \page The IEEE 1394 Reader Class
 
    This text explains how the IEEE 1394 Reader class works.
 
    The IEEE1394Reader object maintains a connection to a DV
    camcorder. It reads DV frames from the camcorder and stores them
    in a queue. The frames can then be retrieved from the buffer and
    displayed, stored, or processed in other ways.
 
    The IEEE1394Reader class supports asynchronous operation: it
    starts a separate thread, which reads as fast as possible from the
    ieee1394 interface card to make sure that no frames are
    lost. Since the buffer can be configured to hold many frames, no
    frames will be lost even if the disk access is temporarily slow.
 
    There are two queues available in an IEEE1394Reader object. One
    queue holds empty frames, the other holds frames filled with DV
    content just read from the interface. During operation the reader
    thread takes unused frames from the inFrames queue, fills them and
    places them in the outFrame queue. The program can then take
    frames from the outFrames queue, process them and finally put
    them back in the inFrames queue.
 
 */

#ifdef HAS_AVC_SUPPORT
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <deque>
#include <iostream>

using std::cerr;
using std::endl;

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libavc1394/rom1394.h>

#include "ieee1394io.h"
#include "frame.h"
#include "error.h"

using namespace avcap;

/** Initializes the IEEE1394Reader object.
 
    The object is initialized with port and channel number. These
    parameters define the interface card and the iso channel on which
    the camcorder sends its data.
 
    The object contains a list of empty frames, which are allocated
    here. 50 frames (2 seconds) should be enough in most cases.
 
    \param c the iso channel number to use
    \param bufSize the number of frames to allocate for the frames buffer
 */

IEEE1394Reader::IEEE1394Reader( int c, int bufSize ) :
	droppedFrames( 0 ),
	incompleteFrames( 0 ),
	currentFrame(0),
	channel( c ),
	isRunning( false )
{
	Frame * frame;

	/* Create empty frames and put them in our inFrames queue */
	for ( int i = 0; i < bufSize; ++i )
	{
		frame = new Frame();
		inFrames.push_back( frame );
	}

	/* Initialize mutexes */
	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init( &mutex, &mutex_attr);
	pthread_mutexattr_destroy(&mutex_attr);
	
	/* Initialise mutex and condition for action triggerring */
	pthread_mutex_init( &condition_mutex, NULL );
	pthread_cond_init( &condition, NULL );

}


/** Destroys the IEEE1394Reader object.
 
    In particular, it deletes all frames in the inFrames and outFrames
    queues, as well as the one currently in use.  Note that one or
    more frames may have been taken out of the queues by a user of the
    IEEE1394Reader class.
 
*/

IEEE1394Reader::~IEEE1394Reader()
{
	Frame * frame;

	for ( int i = inFrames.size(); i > 0; --i )
	{
		frame = inFrames[ 0 ];
		inFrames.pop_front();
		delete frame;
	}
	for ( int i = outFrames.size(); i > 0; --i )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
		delete frame;
	}
	if ( currentFrame != NULL )
	{
		delete currentFrame;
		currentFrame = NULL;
	}
	pthread_mutex_destroy( &condition_mutex );
	pthread_cond_destroy( &condition );
}


/** Fetches the next frame from the output queue
 
    The outFrames contains a list of frames to be processed (saved,
    displayed) by the user of this class.  Copy the first frame
    (actually only a pointer to it) and remove it from the queue.
 
    \note If this returns NULL, wait some time (1/25 sec.) before
    calling it again.
 
    \return a pointer to the current frame, or NULL if no frames are
    in the queue
 
 */

Frame* IEEE1394Reader::GetFrame()
{
	Frame * frame = NULL;

	pthread_mutex_lock( &mutex );

	if ( outFrames.size() > 0 )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
	}
	pthread_mutex_unlock( &mutex );
	if ( frame != NULL )
		frame->ExtractHeader();

	return frame;
}


/** Put back a frame to the queue of available frames
*/

void IEEE1394Reader::DoneWithFrame( Frame* frame )
{
	pthread_mutex_lock( &mutex );
	inFrames.push_back( frame );
	pthread_mutex_unlock( &mutex );
}


/** Return the number of dropped frames since last call
*/

int IEEE1394Reader::GetDroppedFrames( void )
{
	pthread_mutex_lock( &mutex );
	int n = droppedFrames;
	droppedFrames = 0;
	pthread_mutex_unlock( &mutex );
	return n;
}


/** Return the number of incomplete frames since last call
*/

int IEEE1394Reader::GetIncompleteFrames( void )
{
	int n = incompleteFrames;
	incompleteFrames = 0;
	return n;
}


/** Throw away all currently available frames.
 
    All frames in the outFrames queue are put back to the inFrames
    queue.  Also the currentFrame is put back too.  */

void IEEE1394Reader::Flush()
{
	Frame * frame = NULL;

	for ( int i = outFrames.size(); i > 0; --i )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
		inFrames.push_back( frame );
	}
	if ( currentFrame != NULL )
	{
		inFrames.push_back( currentFrame );
		currentFrame = NULL;
	}
}

bool IEEE1394Reader::WaitForAction( int seconds )
{
	pthread_mutex_lock( &mutex );
	int size = outFrames.size( );
	pthread_mutex_unlock( &mutex );

	if ( size == 0 )
	{
		pthread_mutex_lock( &condition_mutex );
		if ( seconds == 0 )
		{
			pthread_cond_wait( &condition, &condition_mutex );
			pthread_mutex_unlock( &condition_mutex );
			pthread_mutex_lock( &mutex );
			size = outFrames.size( );
		}
		else
		{
			struct timeval tp;
			struct timespec ts;
			int result;

			gettimeofday( &tp, NULL );
			ts.tv_sec = tp.tv_sec + seconds;
			ts.tv_nsec = tp.tv_usec * 1000;

			result = pthread_cond_timedwait( &condition, &condition_mutex, &ts );
			pthread_mutex_unlock( &condition_mutex );
			pthread_mutex_lock( &mutex );

			if ( result == ETIMEDOUT )
				size = 0;
			else
				size = outFrames.size();
		}
		pthread_mutex_unlock( &mutex );
	}

	return size != 0;
}

void IEEE1394Reader::TriggerAction( )
{
	pthread_mutex_lock( &condition_mutex );
	pthread_cond_signal( &condition );
	pthread_mutex_unlock( &condition_mutex );
}


/** Initializes the raw1394Reader object.
 
    The object is initialized with port and channel number. These
    parameters define the interface card and the iso channel on which
    the camcorder sends its data.
 
    \param p the number of the interface card to use
    \param c the iso channel number to use
    \param bufSize the number of frames to allocate for the frames buffer
 */


iec61883Reader::iec61883Reader( int p, int c, int bufSize,
	BusResetHandler resetHandler, BusResetHandlerData data ) :
		IEEE1394Reader( c, bufSize ), m_port( p ), m_iec61883dv(0), 
		m_resetHandler( resetHandler),
		m_resetHandlerData( data ),
		m_captureHandler(0)
{
	m_handle = NULL;
}


iec61883Reader::~iec61883Reader()
{
	Close();
}


/** Start receiving DV frames
 
    The ieee1394 subsystem is initialized with the parameters provided
    to the constructor (port and channel).  The received frames can be
    retrieved from the outFrames queue.
 
*/

bool iec61883Reader::StartThread()
{
	if ( isRunning )
		return true;
	pthread_mutex_lock( &mutex );
	currentFrame = NULL;
	if ( Open() && StartReceive() )
	{
		isRunning = true;
		pthread_create( &thread, NULL, ThreadProxy, this );
		pthread_mutex_unlock( &mutex );
		return true;
	}
	else
	{
		Close();
		pthread_mutex_unlock( &mutex );
		return false;
	}
}


/** Stop the receiver thread.
 
    The receiver thread is being canceled. It will finish the next
    time it calls the pthread_testcancel() function.  After it is
    canceled, we turn off iso receive and close the ieee1394
    subsystem.  We also remove all frames in the outFrames queue that
    have not been processed until now.
 
*/

void iec61883Reader::StopThread()
{
	if ( isRunning )
	{
		isRunning = false;
		pthread_join( thread, NULL );
		StopReceive();
		Close();
		Flush();
		TriggerAction( );
	}
}


void iec61883Reader::ResetHandler( void )
{
	if ( m_resetHandler )
		m_resetHandler( const_cast< void* >( m_resetHandlerData ) );
}

int iec61883Reader::ResetHandlerProxy( raw1394handle_t handle, unsigned int generation )
{
	iec61883_dv_t dv = static_cast< iec61883_dv_t >( raw1394_get_userdata( handle ) );
	iec61883_dv_fb_t dvfb = static_cast< iec61883_dv_fb_t >( iec61883_dv_get_callback_data( dv ) );
	iec61883Reader *self = static_cast< iec61883Reader* >( iec61883_dv_fb_get_callback_data( dvfb ) );
	if ( self )
		self->ResetHandler();
	return 0;
}


/** Open the raw1394 interface
 
    \return success/failure
*/

bool iec61883Reader::Open()
{
	bool success;

	assert( m_handle == 0 );

	try
	{
		m_handle = raw1394_new_handle_on_port( m_port );
		if ( m_handle == NULL )
			return false;
		raw1394_set_bus_reset_handler( m_handle, this->ResetHandlerProxy );

		m_iec61883dv = iec61883_dv_fb_init( m_handle, HandlerProxy, this );
		success = ( m_iec61883dv != NULL );
	}
	catch ( string exc )
	{
		Close();
		cerr << exc << endl;
		success = false;
	}
	return success;
}


/** Close the raw1394 interface
 
*/

void iec61883Reader::Close()
{
	if ( m_iec61883dv != NULL )
	{
		iec61883_dv_fb_close( m_iec61883dv );
		m_iec61883dv = NULL;
	}
}

bool iec61883Reader::StartReceive()
{
	bool success;

	/* Starting iso receive */
	try
	{
		fail_neg( iec61883_dv_fb_start( m_iec61883dv, channel ) );
		success = true;
	}
	catch ( string exc )
	{
		cerr << exc << endl;
		success = false;
	}
	return success;
}


void iec61883Reader::StopReceive()
{
	if ( m_iec61883dv != NULL )
	{
		iec61883_dv_fb_close( m_iec61883dv );
		m_iec61883dv = NULL;
	}
}

int iec61883Reader::HandlerProxy( unsigned char *data, int length, int complete, 
	void *callback_data )
{
	iec61883Reader *self = static_cast< iec61883Reader* >( callback_data );
	return self->Handler( length, complete, data );
}

int iec61883Reader::Handler( int length, int complete, unsigned char *data )
{
	pthread_mutex_lock( &mutex );
	if ( currentFrame != NULL )
	{
		outFrames.push_back( currentFrame );
		currentFrame = NULL;
		//cerr << "reader > # inFrames: " << inFrames.size() << ", # outFrames: " << outFrames.size() << endl;
		TriggerAction( );
	}
	if ( inFrames.size() > 0 )
	{
		currentFrame = inFrames.front();
		currentFrame->bytesInFrame = 0;
		inFrames.pop_front();
		//if ( outFrames.size( ) >= 25 )
		//cerr << "reader < # inFrames: " << inFrames.size() << ", # outFrames: " << outFrames.size() << endl;
	}
	else
	{
		droppedFrames++;
		//cerr << "Warning: raw1394 dropped frames: " << droppedFrames << endl;
	}
	pthread_mutex_unlock( &mutex );

	if ( currentFrame != NULL )
	{
		memcpy( currentFrame->data, data, length );
		currentFrame->bytesInFrame = currentFrame->GetFrameSize( );
	}

	if ( !complete )
		incompleteFrames++;

	return 0;
}


/** The thread responsible for polling the raw1394 interface.
 
    Though this is an infinite loop, it can be canceled by StopThread,
    but only in the pthread_testcancel() function.
 
*/
void* iec61883Reader::ThreadProxy( void* arg )
{
	iec61883Reader* self = static_cast< iec61883Reader* >( arg );
	return self->Thread();
}

void* iec61883Reader::Thread()
{
	struct pollfd raw1394_poll;
	int result;
		
	raw1394_poll.fd = raw1394_get_fd( m_handle );
	raw1394_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

	while ( isRunning )
	{
		while ( ( result = poll( &raw1394_poll, 1, 200 ) ) < 0 )
		{
			if ( !( errno == EAGAIN || errno == EINTR ) )
			{
				perror( "error: raw1394 poll" );
				break;
			}
		}
		if ( result > 0 && ( ( raw1394_poll.revents & POLLIN )
		        || ( raw1394_poll.revents & POLLPRI ) ) )
			result = raw1394_loop_iterate( m_handle );
	}
	return NULL;
}


iec61883Connection::iec61883Connection( int port, int node ) :
	m_node( node | 0xffc0 ), m_channel( -1 ), m_bandwidth( 0 ),
	m_outputPort( -1 ), m_inputPort( -1 )
{
	m_handle = raw1394_new_handle_on_port( port );
	if ( m_handle )
	{
		m_channel = iec61883_cmp_connect( m_handle, m_node, &m_outputPort,
			raw1394_get_local_id( m_handle ), &m_inputPort, &m_bandwidth );
		if ( m_channel < 0 )
			m_channel = 63;
	}
}

iec61883Connection::~iec61883Connection( )
{
	if ( m_handle )
	{
		iec61883_cmp_disconnect( m_handle, m_node, m_outputPort,
			raw1394_get_local_id (m_handle), m_inputPort,
			m_channel, m_bandwidth );
		raw1394_destroy_handle( m_handle );
	}
}

void iec61883Connection::CheckConsistency( int port, int node )
{
	raw1394handle_t handle = raw1394_new_handle_on_port( port );
	if ( handle )
		iec61883_cmp_normalize_output( handle, 0xffc0 | node );
	raw1394_destroy_handle( handle );
}


int iec61883Connection::Reconnect( void )
{
	return iec61883_cmp_reconnect( m_handle, m_node, &m_outputPort,
		raw1394_get_local_id( m_handle ), &m_inputPort,
		&m_bandwidth, m_channel );
}

#endif

