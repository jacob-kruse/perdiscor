# Project 4 Description
<p align="center">
  <img src="img/Description_1.png" /><br>
  <img src="img/Description_2.png" /><br>
  <img src="img/Description_3.png" />
</p>

## Instructions
This project implements C code to open a raw TCP socket and manually create the IP and TCP headers with an incremental Time-to-Live (TTL) field to simulate the `traceroute` command. To compile the code, go to the `/Project_4` folder and execute the make command.

    cd ~/perdiscor/Project_4
    make

This will create a program called `tcp_traceroute`. To execute with default values, use the following.

    sudo ./tcp_traceroute

A number of arguments can be passed to this function. These are described below.

* `-m MAX_HOPS`: This determines the maximum number of hops to probe (default: 30)
* `-p DST_PORT`: This determines the destination port to send the traceroute probes (default: 80)
* `-t TARGET`: This determines the destination domain or IP to send the traceroute probes (default: google.com)

For example, if you want to perform tracreoute for `github.com` at port 443, you would use the following command.

    sudo ./tcp_traceroute -p 443 -t github.com

The result of the program will be printed to the terminal in the same format as the `traceroute` command. Below is an example.

    traceroute to github.com (140.82.112.4), 30 hops max, TCP SYN to port 443
     1  _gateway (172.19.52.129)  0.224 ms 0.152 ms 0.134 ms
     2  1057-Life-BAgg-R.net.uga.edu (172.31.43.10)  0.809 ms 0.804 ms 0.796 ms
     3  1023-Boyd-Core-R.net.uga.edu (172.31.40.0)  0.715 ms 0.833 ms 1.153 ms
     4  HSC-Internet-R.net.uga.edu (172.31.40.26)  0.234 ms 0.227 ms 0.221 ms
     5  Boyd-PA-7050.net.uga.edu (172.31.47.9)  0.470 ms 0.234 ms 0.210 ms
     6  HSC-Internet-R.net.uga.edu (172.31.47.21)  0.768 ms 0.447 ms 0.445 ms
     7  128.192.166.44 (128.192.166.44)  4.895 ms 4.390 ms 4.504 ms
     8  143.215.194.194 (143.215.194.194)  4.255 ms 5.274 ms 5.853 ms
     9  * * fourhundredge-0-0-0-1.4079.core2.ashb.net.internet2.edu (163.253.1.134)  15.831 ms
    10  fourhundredge-0-0-0-1.4079.core2.ashb.net.internet2.edu (163.253.1.134)  1.175 ms 0.018 ms 16.056 ms
    11  fourhundredge-0-0-0-1.4079.core2.ashb.net.internet2.edu (163.253.1.134)  1.311 ms  0.034 ms *
    12  * * *
    13  82.221.107.34.bc.googleusercontent.com (34.107.221.82)  157.473 ms 0.050 ms 0.023 ms
    14  * * 93.243.107.34.bc.googleusercontent.com (34.107.243.93)  0.014 ms
    15  * * 93.243.107.34.bc.googleusercontent.com (34.107.243.93)  0.013 ms
    16  lb-140-82-112-4-iad.github.com (140.82.112.4)  19.209 ms 18.960 ms 19.021 ms
