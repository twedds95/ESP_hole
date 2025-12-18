# Ad Blocker for ESP

ESP_hole is a low power, low feature, bloom filter based ad blocking DNS server for the ESP. 

I was inspired by [Rubfi's esphole project](https://github.com/rubfi/esphole) also made for the ESP.

Like Rubfi's project, this uses a modified version of the DNS Server library included with the [Esp8266 core for Arduino](https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer/src)

I created my project using VS Code with the PlatformIO (PIO) extension for the ESP32 S2 mini board. I have included the platformio.ini, as well as the non-default partitions csv file that may need to be modified to match your board. For my board I disabled OTA to allow more block list data.

HARDWARE INFO FOR THIS PROJECT: ESP32S2 240MHz, 320KB RAM, 4MB Flash

## How to install

### Preprocess the hosts file
*I have included my data folder if you do not wish to preprocess your own*

Like Rubfi's project, it is necessary to preprocess the domain block lists. I modified the script to use python, and also added the possibility to run it with multiple source filter lists. It is important to note the ESP's very limited space, so you should monitor the list sizes used and produced. I pivoted from the text comparison approach as I wanted to allow for a larger list of filtered domains while also keeping processing time low. This led me to implement a [Bloom Filter](https://en.wikipedia.org/wiki/Bloom_filter) probabilistic data structure.

    $ python utils/generate_block_lists.py 

Script Output:

    Processed lines : 591,475
    Matched entries : 573,310
    Unique domains  : 422,005

    False Positives %: 1.8289727911195435e-05
    False Positives Estimate: 7.717826225054605
    Bloom built: 421976 domains
    Size: 1525.9 KB

This downloads the hosts file and saves it in different files in the **data_doms** directory. The script then calls **utils/generate_bloom.py** and generates **bloom.bin** in the **data** directory. Depending on the number of domains you are aiming to block, you should modify the *BITS* and *HASHES* values in the **utils/generate_bloom.py** file to minimize the number of false positives your DNS server will block, while also trying to minimize processing time. The way a bloom filter works is it allows for false positives but no false negatives. Therefore, all domains that are included in your lists will always be blocked, but there is also a chance that some not included domains can also get blocked. 

You can run a quick estimate before processing your lists to help you tweak the *BITS* and *HASHES* and make sure the percentage of false positives is acceptable for you by running **utils/estimate_false_positive.py**.

    $ python utils/estimate_false_positive.py 

Script Output:

    Estimated Num Domains: 400000.0
    BITS: 12500000.0
    HASH: 7.0
    False Positives %: 0.0013109989808194958
    False Positives Estimate: 5.243995923277983


You can also make modifications to the generated **hosts_d** files in the **data_doms** directory and run **utils/generate_bloom.py** directly if there are domains that you really want blocked that were not included by the filters. 

A **rewrite** file can also be added in the **data** folder if you wish to add some custom local domains, similar to the Adguard Home DNS rewrites. Each line should contain the domain name (or substring if needed), with a comma and the ip it should point to. The file should also contain a _,@@@_ like the generated block list files. The example below shows that I want the home.page.com domain to point to my local homepage docker instance on 192.168.0.150 (on port 80, so does not need to be specified).

    home.page.com,192.168.0.150
    ,@@@


### Upload the SPIFFS

Upload the **bloom.bin** and (optional) **rewrite** files to the ESP. This is trivial with the PIO extension. But follow [this guide](https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/) if you have issues. Make sure to research your board to determine its specific capacity. 

### Upload the sketch

This version of the project does not require any edits to the source code, as after uploading to the ESP you first run will generate a captive portal for you to input the wifi settings you wish to connect this device to. The captive portal SSID is **AutoConnectAP_ESPHOLE**, and has password **ESP32_Connect** (I guess you could modify these if you wish). 

You may also wish to edit the upstream DNS servers used. These can be edited at the top of *src/ESP_hole.cpp* and are currently using two servers from privacy oriented providers with included filtering to block ads, trackers and phishing websites. 

        IPAddress primaryDNS(94, 140, 14, 14); // adguard
        IPAddress secondaryDNS(194, 242, 2, 4); // mullvad

[**Primary DNS - Adguard**](https://adguard-dns.io/en/public-dns.html)

[**Secondary DNS - Mullvad**](https://mullvad.net/en/help/dns-over-https-and-dns-over-tls) 

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
