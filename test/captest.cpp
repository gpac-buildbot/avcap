/*
 * (c) 2009 Nico Pranke <Nico.Pranke@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2. Refer
 * to the file "COPYING" for details.
 */

#ifndef _WIN32
# include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "avcap/avcap.h"

#include "TestCaptureHandler.h"

#if defined _WIN32 || defined WIN32 || defined _WIN64
# include <windows.h>
# include <winbase.h>
# define usleep(T)	Sleep(T/1000)
# define strcasecmp _stricmp
#endif

using namespace avcap;

typedef struct _optvalues {
	std::string file;
	std::string resolution;
	std::string set_ctrl_string;
	int time;
	int device;
	int format;
	int framerate;
	int input;
	int output;
	bool info;
	bool capture;
	bool help;
	bool list;
	bool set_control;
	bool set_framerate;
	bool set_input;
	bool set_output;
} optvalues;

int parse_options(int argc, char* argv[], optvalues& opts);
void print_usage();
void list_devices();
void capture_data(optvalues& opts);
void set_control(optvalues& opts);
void set_input(optvalues& opts);
void set_output(optvalues& opts);
DeviceDescriptor* get_device_descriptor(int dev_index);

void print_info(int num);
void print_controls(CaptureDevice *dev);
void print_formats(CaptureDevice *dev);
void print_connectors(CaptureDevice *dev);



int main(int argc, char* argv[])
{
	// parse command line arguments and call the proper function
	optvalues opts = {"capture.dat", "", "", 5, 0, 0, 0, 0, 0, false, false, false, false, false, false, false, false};
	if(parse_options(argc, argv, opts) == 0 || opts.help) {
		print_usage();
		return 0;
	}

	if(opts.list) {
		list_devices();
		return 0;
	}

	if(opts.info) {
		print_info(opts.device);
		return 0;
	}

	if(opts.set_control) {
		set_control(opts);
	}

	if(opts.set_input) {
		set_input(opts);
	}

	if(opts.set_output) {
		set_output(opts);
	}

	if(opts.capture) {
		capture_data(opts);
	}

	return 0;
}

int parse_options(int argc, char* argv[], optvalues& opts)
{
     int c;
     int digit_optind = 0;
     int opts_found = 0;

     // parse options
     while (1) {
         int this_option_optind = optind ? optind : 1;
         int option_index = 0;
         static struct option long_options[] = {
             {"list", 0, 0, 'l'},
             {"info", 0, 0, 'i'},
             {"device", 1, 0, 'd'},
             {"capture", 1, 0, 'c'},
             {"time", 1, 0, 't'},
             {"file", 1, 0, 'f'},
             {"format", 1, 0, 'm'},
             {"resolution", 1, 0, 'r'},
             {"set-control", 1, 0, 's'},
             {"set-framerate", 1, 0, 'u'},
             {"set-input", 1, 0, 'j'},
             {"set-output", 1, 0, 'o'},
             {"help", 0, 0, 'h'},
             {0, 0, 0, 0}
         };

         c = getopt_long (argc, argv, "lihcd:t:f:l:r:m:s:u:j:o:",
                  long_options, &option_index);
         if (c == -1)
             break;

         switch (c) {

         // list available devices
         case 'l':
        	 opts.list = true;
        	 opts_found++;
             break;

          // print information about specific device
         case 'i':
             opts.info = true;
        	 opts_found++;
             break;

         // specify device to use
         case 'd':
             opts.device = atoi(optarg);
        	 opts_found++;
             break;

         // capture image data to file
         case 'c':
        	 opts.capture = true;
        	 opts_found++;
             break;

         // specify capture time
         case 't':
        	 opts.time = atoi(optarg);
        	 opts_found++;
        	 break;

         // specify file-name
         case 'f':
             opts.file = optarg;
        	 opts_found++;
             break;

         // specify format
         case 'm':
         	opts.format = atoi(optarg);
         	opts_found++;
         	break;

         // specify resolution
         case 'r':
        	 opts.resolution = optarg;
        	 opts_found++;
        	 break;

         // set control value
         case 's':
        	 opts.set_ctrl_string = optarg;
        	 opts.set_control = true;
        	 opts_found++;
        	 break;

         // set framerate
         case 'u':
        	 opts.set_framerate = true;
        	 opts.framerate = atoi(optarg);
        	 opts_found++;
        	 break;

         // set input-connector
         case 'j':
        	 opts.set_input = true;
        	 opts.input= atoi(optarg);
        	 opts_found++;
        	 break;


         // set output-connector
         case 'o':
        	 opts.set_output = true;
        	 opts.output= atoi(optarg);
        	 opts_found++;
        	 break;

         // print usage
         case 'h':
         case '?':
             opts.help = true;
        	 opts_found++;
             break;

         default:
             break;
         }
     }

     if(opts_found == 0)
    	 std::cout<<"No options found.\n";

     return opts_found;
}

void print_usage()
{
	// print usage and options
	std::cout<<"avcap-library capture test utillity.\n";
	std::cout<<"Options:\n";
	std::cout<<"  -l, --list	: list available capture devices with their indices as used by option -d.\n";
	std::cout<<"  -i, --info	: print detailed information about the specified device.\n";
	std::cout<<"  -d, --device <dev_index>: specify a particular device by its index according to the -l option (default: 0).\n";
	std::cout<<"  -c, --capture : capture from the specified device and save the data to a file.\n";
	std::cout<<"  -f, --file <file-name>: specify a file-name to save the captured data to (default: capture.dat)\n";
	std::cout<<"  -t, --time <time-value>: specify the time to capture in seconds (default: 5 sec).\n";
	std::cout<<"  -m, --format <fmt-index>: set the format prior to start capture according to the index stated by the -i option.\n";
	std::cout<<"  -r, --resolution <res-string>: set the resolution prior to start capture according to the resolution-strings stated by the -i option (e.g. '320x240').\n";
	std::cout<<"  -s, --set-control <ctrl-index>=<value>: set a controls value.\n";
	std::cout<<"  -j, --set-input <input>: set the video input connector.\n";
	std::cout<<"  -o, --set-output <output>: set the video output connector.\n";
	std::cout<<"  -u, --set-framerate <rate>: set the capture frame rate (not supported by all devices).\n";
	std::cout<<"  -h, --help	: print this help and exit.\n";
	std::cout<<"\n";
	std::cout<<"Nico Pranke, TU BA Freiberg, 2008-2009, Nico.Pranke<at>googlemail.com\n";
}

void list_devices()
{
	std::cout<<"\nList of available capture-devices:\n\n";

	// the available devices are queried during instantiation of the DeviceCollector-singleton
	const DeviceCollector::DeviceList& dl = DEVICE_COLLECTOR::instance().getDeviceList();
	DeviceDescriptor* dd = 0;

	if(dl.size() == 0)
		std::cout<<"No capture devices found.\n";

	// iterate through the device-list and print device-names
	int index = 0;
	for(DeviceCollector::DeviceList::const_iterator i = dl.begin(); i != dl.end(); i++) {
		DeviceDescriptor* tmp_dd = *i;
		if(!dd)
			dd = tmp_dd;
		std::cout<<index++<<": Name: "<<tmp_dd->getName()<<std::endl;
	}

	std::cout<<"\n";
}

void print_formats(CaptureDevice * dev)
{
    // list formats
    const FormatManager::ListType & fmt_list = dev->getFormatMgr()->getFormatList();

    // are there some formats
    if(fmt_list.size())
        std::cout << "\nFormats:\n";
    else
        std::cout << "\nNo formats found.\n";

    // iterate over the format-list
    Format *fmt = 0;
    int index = 0;
    for(FormatManager::ListType::const_iterator i = fmt_list.begin();i != fmt_list.end();i++){
        fmt = *i;

        // print name and fourcc
        std::cout << index++ << ": " << fmt->getName() << ", fourcc: " << fmt->getFourcc() << ", Resolutions: ";

        // list resolutions
        int res_count = 0;
        const Format::ResolutionList_t & rl = fmt->getResolutionList();
        for(Format::ResolutionList_t::const_iterator j = rl.begin();j != rl.end();j++){
            Resolution *res = *j;

            // print resolution
            std::cout << res->width << "x" << res->height;
            if(++res_count != rl.size())
                std::cout << ", ";

        }
        std::cout << std::endl;
    }

}

void print_controls(CaptureDevice *dev)
{
	// define printable names
	static const char* type_names[] =
		{"Integer", "Bool", "Button", "Menu", "CtrlClass", "Userdefined" };

	// list controls
    const ControlManager::ListType & cl = dev->getControlMgr()->getControlList();

    // are there some controls
    if(cl.size())
        std::cout << "\nControls:\n";
    else
        std::cout << "\nNo controls found.\n";

    // iterate over the list of controls
    int ctrl_index = 0;
    for(ControlManager::ListType::const_iterator i = cl.begin();i != cl.end();i++, ctrl_index++){
        Control *ctrl = *i;

        // dump control value and type
        std::cout << ctrl_index << ": " << ctrl->getName() << ", type: " << type_names[ctrl->getType()] << ", id: " << ctrl->getId() << ", value: " << ctrl->getValue() << ", default: " << ctrl->getDefaultValue();

        // dump items for menu-like controls
        if(ctrl->getType() == Control::MENU_CONTROL){
            std::cout << ", Items: ";

            // print the menu-items
            const MenuControl::ItemList & item_list = ((MenuControl*)((ctrl)))->getItemList();
            for(MenuControl::ItemList::const_iterator j = item_list.begin();j != item_list.end();j++){
                MenuItem *item = *j;
                std::cout << "(" << item->index << ": " << item->name << ") ";
            }
        }

        // dump range for integer controls
        if(ctrl->getType() == Control::INTEGER_CONTROL){
            const Interval & intv = ((IntegerControl*)((ctrl)))->getInterval();
            std::cout << ", min: " << intv.min << ", max: " << intv.max << ", step: " << intv.step;
        }

        std::cout << std::endl;
    }
}

void print_connectors(CaptureDevice *dev)
{
    // list inputs
    const ConnectorManager::ListType & icl = dev->getConnectorMgr()->getVideoInputList();
    if(icl.size())
        std::cout << "\nInput-Connectors: \n";

    else
        std::cout << "\nNo input-connectors found. \n";

    // iterate over the input connector-list
    for(ConnectorManager::ListType::const_iterator i = icl.begin();i != icl.end();i++){
        Connector *conn = *i;

        //dump index and name
        std::cout << conn->getIndex() << ": " << conn->getName();
        if(dev->getConnectorMgr()->getVideoInput()->getIndex() == conn->getIndex())
            std::cout << " *";
        std::cout << "\n";
    }
    // list outputs
    const ConnectorManager::ListType & ocl = dev->getConnectorMgr()->getVideoOutputList();

    // are there outputs?
    if(ocl.size())
        std::cout << "\nOutput-Connectors: \n";
    else
        std::cout << "\nNo output-connectors found. \n";

    // iterate over the output connector-list
    for(ConnectorManager::ListType::const_iterator i = ocl.begin();i != ocl.end();i++){
        Connector *conn = *i;

        // dump index and name
        std::cout << conn->getIndex() << ": " << conn->getName();
        if(dev->getConnectorMgr()->getVideoOutput()->getIndex() == conn->getIndex())
            std::cout << " *";
        std::cout << "\n";
    }
}

void print_info(int num)
{
	DeviceDescriptor*dd = get_device_descriptor(num);

	if(dd) {
		// print detailed information about a device
		std::cout<<"\nDetailed information for capture-device: "<<num<<"\n\n";
		std::cout<<"Name: "<<dd->getName();

		if(dd->getDriver() != "")
			std::cout<<", Driver: "<<dd->getDriver();
		if(dd->getCard() != "")
			std::cout<<", Device: "<<dd->getCard();
		if(dd->getInfo() != "")
			std::cout<<", Info: "<<dd->getInfo();

		std::cout<<std::endl;

		// open the device descriptor and get the associated device
		dd->open();
		CaptureDevice* dev = dd->getDevice();

		if(!dev) {
			std::cerr<<"Failed to create CaptureDevice-object for device: "<<dd->getName()<<std::endl;
			exit(0);
		}

		// print format-list, controls and connectors
		print_formats(dev);
		print_controls(dev);
		print_connectors(dev);

		dd->close();
	} else {
		std::cout<<"Device with given index not found.\n";
	}

	std::cout<<"\n";
}

void capture_data(optvalues& opts)
{
	// get the descriptor
	DeviceDescriptor*dd = get_device_descriptor(opts.device);

	if(dd) {
		// open the descriptor and get the associated device
		dd->open();
		CaptureDevice* dev = dd->getDevice();

		if(!dev) {
			std::cerr<<"Failed to create CaptureDevice-object for device: "<<dd->getName()<<std::endl;
			exit(0);
		}

		// set the format
		Format* fmt = 0;
		const FormatManager::ListType& fmt_list = dev->getFormatMgr()->getFormatList();

		if(fmt_list.size()) {

			// find the desired format
			int index = 0;
			for(FormatManager::ListType::const_iterator i = fmt_list.begin(); i != fmt_list.end(); i++, index++) {
				fmt = *i;
				if(index == opts.format) break;
			}

			// and set it, if found
			if(fmt) {
				dev->getFormatMgr()->setFormat(fmt);
				fmt = dev->getFormatMgr()->getFormat();
			}

			// dump the actually set format
			fmt = dev->getFormatMgr()->getFormat();
			if(fmt)
				std::cout<<"Format: "<<fmt->getName()<<"\n";
			else {
				std::cerr<<"Setting desired format failed, trying default.\n";
			}
		}

		if(fmt) {
			// set the desired resolution
			int width = 0, height = 0;
			if(opts.resolution != "") {
				sscanf(opts.resolution.c_str(), "%dx%d", &width, &height);
				dev->getFormatMgr()->setResolution(width, height);
			}

			// and dump the new resolution
			width = dev->getFormatMgr()->getWidth();
			height = dev->getFormatMgr()->getHeight();
			std::cout<<"Resolution: "<<width<<"x"<<height<<"\n";
		}

		// set the framerate if specified
		if(opts.set_framerate)
			dev->getFormatMgr()->setFramerate(opts.framerate);

		// create and register the capture handler
		TestCaptureHandler cap_handler(opts.file);
		dev->getVidCapMgr()->registerCaptureHandler(&cap_handler);

		std::cout<<"\nCapturing "<<opts.time<<" seconds of data from device "<<opts.device<<""
				", saving to '"<<opts.file<<"'.\n\n";

		// and capture specified time
		if(dev->getVidCapMgr()->startCapture() != -1) {
			usleep(1000*1000*opts.time);
			dev->getVidCapMgr()->stopCapture();
		}

		std::cout<<"\n";
		dd->close();
	}
}

DeviceDescriptor* get_device_descriptor(int dev_index)
{
	// find the descriptor of the device in the device-list by index
	const DeviceCollector::DeviceList& dl = DEVICE_COLLECTOR::instance().getDeviceList();
	DeviceDescriptor* dd = 0;
	int index = 0;

	if(dev_index >= dl.size() || dev_index < 0) {
		std::cout<<"No device found with index: "<<dev_index<<"\n";
		return 0;
	}

	for(DeviceCollector::DeviceList::const_iterator i = dl.begin(); i != dl.end(); i++, index++) {
		dd = *i;
		if(index == dev_index) break;
	}

	return dd;
}


void set_control(optvalues& opts)
{
	std::string str = opts.set_ctrl_string;
	std::string::size_type assignment_pos = str.find('=');
	int ctrl_index = -1;
	bool assignment_found = assignment_pos == std::string::npos ? false : true;

	// divide the option string into left and right hand side of the assignment
	std::string index_str = str.substr(0, assignment_pos);
	std::string value_str = str.substr(assignment_pos + 1, str.size() - assignment_pos - 1);

	// check if index-string is realy a number
	if(isdigit(index_str.at(0)))
		ctrl_index = atoi(index_str.c_str());

	// get the descriptor and open it
	DeviceDescriptor*dd = get_device_descriptor(opts.device);
	if(!dd)	return;
	dd->open();

	// find the control in question
	const ControlManager::ListType& cl = dd->getDevice()->getControlMgr()->getControlList();

	if(ctrl_index >= cl.size() || ctrl_index < 0) {
		std::cout<<"\nControl-Index: "<<ctrl_index<<" out of range.\n";
		return;
	}

	Control* ctrl = 0;
	int index = 0;
	for(ControlManager::ListType::const_iterator i = cl.begin(); i != cl.end(); index++, i++) {
		ctrl = *i;
		if(index == ctrl_index) break;
	}

	bool modified = false;

	// its an integer-control
	if(ctrl->getType() == Control::INTEGER_CONTROL) {
		// so do some check of the value-string
		if(value_str.size() == 0)
			std::cout<<"set control: No value given.\n";
		else if(isdigit(value_str.at(0)) || value_str.at(0) == '-') {
			// set the new value
			int value = atoi(value_str.c_str());
			ctrl->setValue(value);
			modified = true;
		}
	}

	// its a menu-control
	if(ctrl->getType() == Control::MENU_CONTROL) {
		// check the value-string
		if(value_str.size() == 0)
			std::cout<<"set control: No value given.\n";
		else if(isdigit(value_str.at(0))) {
			// set the value
			int value = atoi(value_str.c_str());
			ctrl->setValue(value);
			modified = true;
		}
	}

	// its a bool-control
	if(ctrl->getType() == Control::BOOL_CONTROL) {
		bool value = false;
		bool value_found = false;

		// allowed values are: true, false, on, off, 1 or 0 (case independent)
		if(value_str.size() == 0)
			std::cout<<"set control: No value given.\n";
		else if (strcmp(value_str.c_str(), "true") == 0 ||
			strcmp(value_str.c_str(), "on") == 0 ||
			value_str.at(0) == '1') {
			value = true;
			value_found = true;
		} else if(strcmp(value_str.c_str(), "false") == 0 ||
			strcmp(value_str.c_str(), "off" ) == 0 ||
			value_str.at(0) == '0') {
			value = false;
			value_found = true;
		}

		// set the value if a valid value was found
		if(value_found) {
			ctrl->setValue(value);
			modified = true;
		}
	}

	// its a button-control, so push it
	if(ctrl->getType() == Control::BUTTON_CONTROL) {
		((ButtonControl*) ctrl)->push();
		modified = true;
	}

	if(modified)
		std::cout<<"set control: "<<ctrl->getName()<<", new value: "<<ctrl->getValue()<<"\n";

	dd->close();
}


void set_input(optvalues& opts)
{
	// get the device descriptor
	DeviceDescriptor*dd = get_device_descriptor(opts.device);
	if(!dd)	return;
	dd->open();

	// the list of input-connectors
	const ConnectorManager::ListType& icl = dd->getDevice()->getConnectorMgr()->getVideoInputList();

	// and find the desired connector-object
	Connector* conn = 0;
	for(ConnectorManager::ListType::const_iterator i = icl.begin(); i != icl.end(); i++) {
		if((*i)->getIndex() == opts.input) {
			conn = *i;
			break;
		}
	}

	// set the new input, if found
	if(!conn)
		std::cout<<"Connector not found.\n";
	else {
		dd->getDevice()->getConnectorMgr()->setVideoInput(conn);
		std::cout<<"set video input connector: "<<conn->getName()<<"\n";
	}

	dd->close();
}

void set_output(optvalues& opts)
{
	// get the device-descriptor
	DeviceDescriptor*dd = get_device_descriptor(opts.device);
	if(!dd)	return;
	dd->open();

	// the output connector-list
	const ConnectorManager::ListType& icl = dd->getDevice()->getConnectorMgr()->getVideoOutputList();

	// find the connector
	Connector* conn = 0;
	for(ConnectorManager::ListType::const_iterator i = icl.begin(); i != icl.end(); i++) {
		if((*i)->getIndex() == opts.output) {
			conn = *i;
			break;
		}
	}

	// and set the output
	if(!conn)
		std::cout<<"Connector not found.\n";
	else {
		dd->getDevice()->getConnectorMgr()->setVideoOutput(conn);
		std::cout<<"set video output connector: "<<conn->getName()<<"\n";
	}

	dd->close();
}

