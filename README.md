# Ad Blocker for ESP

ESP_hole is a low power, low feature, ad blocking DNS server for the ESP32. 

I was heavily inspired by [Rubfi's esphole project](https://github.com/rubfi/esphole) also made for the ESP.

Like Rubfi's project, this uses a modified version of the DNS Server library included with the [Esp8266 core for Arduino](https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer/src)

I created my project using VS Code with the PlatformIO (PIO) extension for the ESP32 S2 mini board. I have included the platformio.ini, as well as the non-default partitions csv file that may need to be modified to match your board. For my board I disabled OTA to allow more block list data.

HARDWARE INFO FOR THIS PROJECT: ESP32S2 240MHz, 320KB RAM, 4MB Flash

## How to install

### Preprocess the hosts file
Like Rubfi's project, it is necessary to preprocess the domain block lists. I modified the script to use python, and also added the possibility to run it with multiple source filter lists. It is important to note the ESP's very limited space, so you should monitor the list sizes used and produced. I had to comment some of the filter lists because they would produce too much data for the ESP to handle. 

    $ python utils/generate_block_lists.py 

This downloads the hosts file and saves it in different files in the *data* directory.
As reference, for the ESP32 S2 mini, I was aiming to keep the total number of Unique domains below 100,000. Like Rubfi's project, the host files are preprocessed and stored in smaller files based on the lenght of the domain. Due to using more, and larger lists, the script will also further break down the data into smaller files based on length of the domain as well as the first letter if the data for a particular length becomes too large. The ESP code handles this flexible file lookup logic. 

Script Output:

    Processed lines : 139,205
    Matched entries : 121,068
    Unique domains  : 93,434


You can also add a **rewrite** file in the data folder if you wish to add some custom local domains, similar to the Adguard Home DNS rewrites. Each line should contain the domain name (or substring if needed), with a comma and the ip it should point to. The file should also contain a _,@@@_ like the generated block list files. The example below shows that I want the home.page.com domain to point to my local homepage docker instance on 192.168.0.150 (on port 80, so does not need to be specified).

    home.page.com,192.168.0.150
    ,@@@


### Upload the SPIFFS

Trivial with the PIO extension. But follow [this guide](https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/) if you have issues. Make sure to research your board to determine its specific capacity. 

### Upload the sketch

This version of the project does not require any edits to the source code, as after uploading to the ESP you first run will generate a captive portal for you to input the wifi settings you wish to connect this device to. The captive portal SSID is **AutoConnectAP_ESPHOLE**, and has password **ESP32_Connect** (I guess you could modify these if you wish). 

You may also wish to edit the upstream DNS servers used. These can be edited at the top of *src/ESP_hole.cpp* and are currently using two ad blocking and privacy oriented providers.

        IPAddress primaryDNS(94, 140, 14, 140); // adguard
        IPAddress secondaryDNS(194, 242, 2, 4); // mullvad

## Usage/Test

Open the serial monitor or minicom in order to find the IP of the ESP device (or scan your network in order to find it) and use nslookup in order to test some domains:

    $ nslookup - 192.168.0.161
    > github.com
    Server:		192.168.0.161
    Address:	192.168.0.161#53

    Non-authoritative answer:
    Name:	github.com
    Address: 192.30.253.112

    > analytics.google.com
    Server:		192.168.0.161
    Address:	192.168.0.161#53

    Non-authoritative answer:
    Name:	analytics.google.com
    Address: 0.0.0.0

In the serial monitor the debug information is shown:

    Domain: c.amazon-adsystem.com Blocked | Find took 69 ms

    Domain: c.amazon-adsystem.com Blocked | Find took 69 ms

    Domain: d.pub.network Blocked | Find took 0 ms

    Domain: d.pub.network Blocked | Find took 1 ms

    Domain: google.com | IP:142.250.69.78
    Resolv took 31 ms | Find took 19 ms

    Domain: beacons5.gvt3.com Blocked | Find took 1 ms

    Domain: beacons4.gvt2.com Blocked | Find took 1 ms

    Domain: home.page.com | IP:192.168.0.150
    Rewrite took 37 ms
