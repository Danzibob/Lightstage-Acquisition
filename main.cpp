#include <iomanip>  // std:setw
#include <mutex>
#include <thread>

#include "ArenaApi.h"
#include "SaveApi.h"

#define TAB1 "  "
#define TAB2 "    "
#define TAB3 "      "

#define EXPOSURE_TIME 44000.0
#define DELTA_TIME 1000.0
#define PTP_SYNC 1

#define FILE_NAME "Images/image_"
// https://support.thinklucid.com/knowledgebase/pixel-formats/
#define PIXEL_FORMAT BGR8

// readable identifier for the thread writing to std::cout
#define THREAD_INFO std::setw(20) << threadName << " | "

// for making cout thread safe
std::mutex g_print_mtx;

// thread safe print statement
#define PRINT_LOCK(statement)                              \
    {                                                      \
        {                                                  \
            std::lock_guard<std::mutex> lock(g_print_mtx); \
            statement;                                     \
        }                                                  \
    }

void SaveImageRAW(Arena::IImage *image, const char *filename) {
    // prepare image parameters
    std::cout << "Prepare image parameters\n";
    Save::ImageParams params(
        image->GetWidth(),
        image->GetHeight(),
        image->GetBitsPerPixel());

    // prepare image writer
    std::cout << "Prepare image writer\n";
    Save::ImageWriter writer(params, filename);

    // Set image writer to RAW
    std::cout << "Set image writer to RAW\n";
    writer.SetRaw(".raw");

    // save image
    std::cout << "Save image\n";
    writer << image->GetData();
}

void SaveImagePNG(Arena::IImage *image, const char *filename) {
    // convert image
    std::cout << TAB1 << "Convert image to " << GetPixelFormatName(PIXEL_FORMAT) << "\n";

    auto pConverted = Arena::ImageFactory::Copy(
        image);

    // prepare image parameters
    std::cout << TAB1 << "Prepare image parameters\n";

    Save::ImageParams params(
        pConverted->GetWidth(),
        pConverted->GetHeight(),
        pConverted->GetBitsPerPixel());

    // prepare image writer
    std::cout << TAB1 << "Prepare image writer\n";

    Save::ImageWriter writer(
        params,
        filename);

    // Set image writer to PNG
    std::cout << TAB1 << "Set image writer to PNG\n";
    writer.SetPng(".png", 0, false);

    // save image
    std::cout << TAB1 << "Save image at " << filename << std::endl;
    writer << pConverted->GetData();

    // destroy converted image
    Arena::ImageFactory::Destroy(pConverted);
}

void shoot(Arena::ISystem *system, std::vector<Arena::IDevice *> devices) {
    std::cout << "There are " << devices.size() << " cameras!\n";

    // Start streams
    //    Start the stream before grabbing any images. Starting the stream
    //    allocates buffers, which can be passed in as an argument (default: 10),
    //    and begins filling them with data. Starting the stream blocks write
    //    access to many features such as width, height, and pixel format, as
    //    well as acquisition and buffer handling modes, among others. The stream
    //    needs to be stopped later.
    std::cout << "\n"
              << TAB1 << "Start streams\n";
    for (size_t i = 0; i < devices.size(); i++) {
        devices.at(i)->StartStream();
    }

    // Set up timing and broadcast action command
    //    Action commands must be scheduled for a time in the future. This can be
    //    done by grabbing the PTP time from a device, adding a delta to it, and
    //    setting it as an action command's execution time.
    std::cout << TAB1 << "Set action command to " << DELTA_TIME << " nanoseconds from now\n";

    // execute latch
    Arena::ExecuteNode(
        devices.at(0)->GetNodeMap(),
        "PtpDataSetLatch");

    // get latch
    int64_t ptpDataSetLatchValue = Arena::GetNodeValue<int64_t>(
        devices.at(0)->GetNodeMap(),
        "PtpDataSetLatchValue");

    // set execute time to future time
    Arena::SetNodeValue<int64_t>(
        system->GetTLSystemNodeMap(),
        "ActionCommandExecuteTime",
        ptpDataSetLatchValue + DELTA_TIME);

    // Fire action command
    //    Action commands are fired and broadcast to all devices, but only
    //    received by the devices matching desired settings.
    std::cout << TAB1 << "Fire action command\n";
    Arena::ExecuteNode(
        system->GetTLSystemNodeMap(),
        "ActionCommandFireCommand");

    // get images and check timestamps
    std::cout << TAB1 << "Get images\n";

    for (size_t i = 0; i < devices.size(); i++) {
        Arena::IDevice *pDevice = devices.at(i);
        GenICam::gcstring deviceSerialNumber = Arena::GetNodeValue<GenICam::gcstring>(pDevice->GetNodeMap(), "DeviceSerialNumber");
        std::cout << TAB2 << "Image from device " << deviceSerialNumber << "\n";

        // Compare timestamps
        //    Scheduling action commands amongst PTP synchronized devices results
        //    in synchronized images with synchronized timestamps.
        std::cout << TAB3 << "Timestamp: ";

        // Initiate image transfer from current camera
        Arena::ExecuteNode(pDevice->GetNodeMap(), "TransferStart");
        Arena::IImage *pImage = pDevice->GetImage(3000);
        Arena::ExecuteNode(pDevice->GetNodeMap(), "TransferStop");
        std::cout << pImage->GetTimestamp() << "\n";

        std::ostringstream png_file, raw_file;
        png_file << FILE_NAME << deviceSerialNumber << ".png";
        raw_file << FILE_NAME << deviceSerialNumber << ".raw";
        SaveImagePNG(pImage, png_file.str().c_str());
        SaveImageRAW(pImage, raw_file.str().c_str());
        // requeue buffer
        pDevice->RequeueBuffer(pImage);
    }

    // stop stream
    std::cout << TAB1 << "Stop stream\n";
    for (size_t i = 0; i < devices.size(); i++) {
        devices.at(i)->StopStream();
    }
}

void wait_for_ptp_sync(std::vector<Arena::IDevice *> devices) {
    std::vector<GenICam::gcstring> serials;
    int i = 0;
    do {
        bool masterFound = false;
        bool restartSyncCheck = false;
        // check devices
        for (size_t j = 0; j < devices.size(); j++) {
            Arena::IDevice *pDevice = devices.at(j);
            // get PTP status
            GenICam::gcstring ptpStatus = Arena::GetNodeValue<GenICam::gcstring>(pDevice->GetNodeMap(), "PtpStatus");
            if (ptpStatus == "Master") {
                if (masterFound) {
                    // Multiple masters -- ptp negotiation is not complete
                    restartSyncCheck = true;
                    break;
                }
                masterFound = true;
            } else if (ptpStatus != "Slave") {
                // Uncalibrated state -- ptp negotiation is not complete
                restartSyncCheck = true;
                break;
            }
        }
        // A single master was found and all remaining cameras are slaves
        if (!restartSyncCheck && masterFound)
            break;
        std::this_thread::sleep_for(std::chrono::duration<int>(1));
        // for output
        if (i % 10 == 0)
            std::cout << "\r"
                      << "                    "
                      << "\r" << TAB2 << std::flush;
        std::cout << "." << std::flush;
        i++;
    } while (true);
}

void sync_and_prep(Arena::ISystem *system, std::vector<Arena::IDevice *> devices) {
    std::cout << TAB1 << "Sync & Prep\n";

    for (size_t i = 0; i < devices.size(); i++) {
        Arena::IDevice *device = devices.at(i);
        GenICam::gcstring deviceSerialNumber = Arena::GetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "DeviceSerialNumber");
        std::cout << TAB2 << "Prepare camera " << deviceSerialNumber << "\n";

        // Manually set exposure time
        //    In order to get synchronized images, the exposure time must be
        //    synchronized as well.
        std::cout << TAB3 << "Exposure: ";

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "ExposureAuto",
            "Off");

        Arena::SetNodeValue<double>(
            device->GetNodeMap(),
            "ExposureTime",
            EXPOSURE_TIME);

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "GainAuto",
            "Off");

        Arena::SetNodeValue<double>(
            device->GetNodeMap(),
            "Gain",
            12.0);

        std::cout << Arena::GetNodeValue<double>(device->GetNodeMap(), "ExposureTime") << "\n";

        // Enable trigger mode and set source to action
        //    To trigger a single image using action commands, trigger mode must
        //    be enabled, the source set to an action command, and the selector
        //    set to the start of a frame.
        std::cout << TAB3 << "Trigger: ";

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "TriggerSelector",
            "FrameStart");

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "TriggerMode",
            "On");

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "TriggerSource",
            "Action0");

        std::cout << Arena::GetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "TriggerSource") << "\n";

        // Prepare the device to receive an action command
        //    Action unconditional mode allows a camera to accept action from an
        //    application without write access. The device key, group key, and
        //    group mask must match similar settings in the system's TL node map.
        std::cout << TAB3 << "Action commands: ";

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "ActionUnconditionalMode",
            "On");

        Arena::SetNodeValue<int64_t>(
            device->GetNodeMap(),
            "ActionSelector",
            0);

        Arena::SetNodeValue<int64_t>(
            device->GetNodeMap(),
            "ActionDeviceKey",
            1);

        Arena::SetNodeValue<int64_t>(
            device->GetNodeMap(),
            "ActionGroupKey",
            1);

        Arena::SetNodeValue<int64_t>(
            device->GetNodeMap(),
            "ActionGroupMask",
            1);

        std::cout << "prepared\n";

        // Synchronize devices by enabling PTP
        //    Enabling PTP on multiple devices causes them to negotiate amongst
        //    themselves so that there is a single master device while all the
        //    rest become slaves. The slaves' clocks all synchronize to the
        //    master's clock.
        std::cout << TAB3 << "PTP: ";

        Arena::SetNodeValue<bool>(
            device->GetNodeMap(),
            "PtpEnable",
            true);

        std::cout << (Arena::GetNodeValue<bool>(device->GetNodeMap(), "PtpEnable") ? "enabled" : "disabled") << "\n";

        // Use max supported packet size. We use transfer control to ensure that only one camera
        //    is transmitting at a time.
        Arena::SetNodeValue<bool>(device->GetTLStreamNodeMap(), "StreamAutoNegotiatePacketSize", true);

        // enable stream packet resend
        Arena::SetNodeValue<bool>(device->GetTLStreamNodeMap(), "StreamPacketResendEnable", true);

        // Enable user controlled transfer control
        //    Synchronized cameras will begin transmiting images at the same
        //    time. To avoid missing packets due to collisions, we will use
        //    transfer control to control when each camera transmits the image.
        std::cout << TAB3 << "Transfer Control: ";

        Arena::SetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "TransferControlMode", "UserControlled");
        Arena::SetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "TransferOperationMode", "Continuous");
        Arena::ExecuteNode(device->GetNodeMap(), "TransferStop");

        std::cout << Arena::GetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "TransferControlMode") << " - "
                  << Arena::GetNodeValue<GenICam::gcstring>(device->GetNodeMap(), "TransferOperationMode") << " - "
                  << "Transfer Stopped\n";

        std::cout << TAB1 << "Set pixel format to BayerRG12p\n";

        Arena::SetNodeValue<GenICam::gcstring>(
            device->GetNodeMap(),
            "PixelFormat",
            "BayerRG16");
    }

    // Overall system prep
    // prepare system
    std::cout << TAB2 << "Prepare system\n";

    // Prepare the system to broadcast an action command
    //    The device key, group key, group mask, and target IP must all match
    //    similar settings in the devices' node maps. The target IP acts as a
    //    mask.
    std::cout << TAB3 << "Action commands: ";

    Arena::SetNodeValue<int64_t>(
        system->GetTLSystemNodeMap(),
        "ActionCommandDeviceKey",
        1);

    Arena::SetNodeValue<int64_t>(
        system->GetTLSystemNodeMap(),
        "ActionCommandGroupKey",
        1);

    Arena::SetNodeValue<int64_t>(
        system->GetTLSystemNodeMap(),
        "ActionCommandGroupMask",
        1);

    Arena::SetNodeValue<int64_t>(
        system->GetTLSystemNodeMap(),
        "ActionCommandTargetIP",
        0xFFFFFFFF);

    std::cout << "prepared\n";

    // Wait for devices to negotiate their PTP relationship
    //    Before starting any PTP-dependent actions, it is important to wait for
    //    the devices to complete their negotiation; otherwise, the devices may
    //    not yet be synced. Depending on the initial PTP state of each camera,
    //    it can take about 40 seconds for all devices to autonegotiate. Below,
    //    we wait for the PTP status of each device until there is only one
    //    'Master' and the rest are all 'Slaves'. During the negotiation phase,
    //    multiple devices may initially come up as Master so we will wait until
    //    the ptp negotiation completes.
    if (PTP_SYNC) {
        std::cout << TAB1 << "Wait for devices to negotiate. This can take up to a minute.\n";
        wait_for_ptp_sync(devices);
        std::cout << TAB1 << "PTP Sync established\n";
    }
}

int main() {
    std::cout << "Dome Acquisition Test\n";

    try {
        // Initialize SDK
        Arena::ISystem *system = Arena::OpenSystem();
        // Get network devices with a timeout in ms
        system->UpdateDevices(1000);
        std::vector<Arena::DeviceInfo> deviceInfos = system->GetDevices();

        // Warn & exit if no cameras found
        if (deviceInfos.size() == 0) {
            std::cout << "\nNo camera connected, exiting.\n";
            return 0;
        }

        // Create all discovered cameras and add them to a vector
        std::vector<Arena::IDevice *> vDevices = std::vector<Arena::IDevice *>();
        for (auto &deviceInfo : deviceInfos) {
            Arena::IDevice *device = system->CreateDevice(deviceInfo);
            vDevices.push_back(device);
        }

        // prep the devices
        std::cout << "Commence prep\n\n";
        sync_and_prep(system, vDevices);
        std::cout << "\nPrep complete\n";

        // do stuff with the devices
        std::cout << "Commence stuff\n\n";
        shoot(system, vDevices);
        std::cout << "\nStuff complete\n";

        // clean up example
        for (auto &device : vDevices) {
            system->DestroyDevice(device);
        }

    } catch (GenICam::GenericException &ge) {
        std::cout << "\nGenICam exception thrown: " << ge.what() << "\n";
        return -1;
    } catch (std::exception &ex) {
        std::cout << "\nStandard exception thrown: " << ex.what() << "\n";
        return -1;
    } catch (...) {
        std::cout << "\nUnexpected exception thrown\n";
        return -1;
    }

    return 0;
}