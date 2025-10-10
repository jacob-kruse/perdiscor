#!/usr/bin/env python3

"""
This script acts as DNS Forwarder that blocks requests to specified domains
"""

import requests
from argparse import ArgumentParser
from csv import reader, writer
from scapy.sendrecv import sniff, sr1
from socket import socket, AF_INET, SOCK_DGRAM
from scapy.layers.dns import DNS, DNSQR, IP, UDP, dnsqtypes


def main():
    # Define arguments
    desc = "A DNS forwarder with domain blocking and DoH capabilities"
    parser = ArgumentParser(description=desc)
    parser.add_argument("-d", dest="dst_ip", default="1.1.1.1", help="Destination DNS server IP")
    parser.add_argument("-f", dest="deny_list_file", default="deny_list.csv", help="File containing domains to block")
    parser.add_argument("-l", dest="log_file", default="log.csv", help="Append-only log file")
    parser.add_argument("--doh", dest="doh", action="store_true", help="Use default upstream DoH server")
    parser.add_argument("--doh_server", dest="doh_server", default="192.1.1.1", help="Use this upstream DoH server")

    # Assign arguments to variables
    args = parser.parse_args()
    dst_ip = args.dst_ip
    deny_list_file = args.deny_list_file
    log_file = args.log_file
    doh = args.doh
    doh_server = args.doh_server

    # Print arguments
    print("Arguments\n----------")
    print(f"Destination IP: {dst_ip}")
    print(f"Deny List File: {deny_list_file}")
    print(f"Log File: {log_file}")
    print(f"DoH: {doh}")
    print(f"DoH Server: {doh_server}\n")

    # Initialize DNSForwarder Class and call the listen() function
    dns = DNSForwarder(dst_ip, deny_list_file, log_file, doh, doh_server)
    dns.listen()


class DNSForwarder:
    def __init__(self, dst_ip, deny_list_file, log_file, doh, doh_server):
        # Assign global variables
        self.dst_ip = dst_ip
        self.deny_list_file = deny_list_file
        self.log_file = log_file
        self.doh = doh
        self.doh_server = doh_server

    def begin_sniff(self):
        # Begin to sniff packets
        sniff(prn=self.sniff_callback, store=False)

    def sniff_callback(self, msg):
        # If the sniffed packet has DNS and DNS Query sections and the if the message is a query not reply (qr = 0)
        if msg.haslayer(DNS) and msg.haslayer(DNSQR) and msg[DNS].qr == 0:
            print(msg[DNSQR].qname.decode().rstrip("."))
            print(msg[DNSQR].qtype)
            qtype = msg[DNSQR].qtype

            # Extract the qname from the DNS Query part of the message and convert from bytes for the domain
            domain = msg[DNSQR].qname.decode().rstrip(".")

            # Pass the domain to the check_domain() function to see if it is blocked
            blocked = self.check_domain(domain)

            # After determining the status of the desired domain, add to the logger
            self.log_domain(domain, blocked, "A")

    def listen(self):
        # Create sockets, AF_INET=IPv4, SOCK_DGRAM=UDP
        temp_sock = socket(AF_INET, SOCK_DGRAM)
        req_sock = socket(AF_INET, SOCK_DGRAM)
        dns_sock = socket(AF_INET, SOCK_DGRAM)

        # Connect to the temporary socket, get the socket name to extract the device's IP, and close the socket
        temp_sock.connect(("8.8.8.8", 80))
        local_ip = temp_sock.getsockname()[0]
        temp_sock.close()

        # Bind the request socket to this device's IP and port 53 (Port for DNS requests)
        req_sock.bind((local_ip, 53))

        # Start loop
        while True:
            # Listen for DNS requests to the server
            req, addr = req_sock.recvfrom(512)

            # Transform the raw bytes into a readable DNS message and get the domain and query type from the request
            dns_req = DNS(req)
            domain = dns_req[DNSQR].qname.decode().rstrip(".")
            qt = dns_req[DNSQR].qtype
            q_type = dnsqtypes.get(qt, str(qt))
            print(f"Received DNS Query for '{domain}' {q_type}")

            # Check if the domain is blocked
            blocked = self.check_domain(domain)

            # Send the DNS request to the upstream DNS server, and listen for a response
            print("Forwarding DNS Query")
            dns_sock.sendto(req, (self.dst_ip, 53))
            resp, _ = dns_sock.recvfrom(512)
            print("Received from DNS Resolver")

            # Log whether or not the domain was blocked and the query type
            self.log_domain(domain, blocked, q_type)

            # Forward the DNS response back to the client
            req_sock.sendto(resp, addr)

    def check_domain(self, domain):
        # Define the blocked variable to be False
        blocked = False

        # Open the file with the denied domains
        with open(self.deny_list_file, "r") as file:
            # Define a reader for the file
            read_file = reader(file)

            # Iterate through each row in the file
            for row in read_file:
                # If the domain is equal to one of the blocked domains
                if domain == row[0]:
                    # Indicate that the site is blocked and break the "for" loop
                    blocked = True
                    break

        return blocked

    def log_domain(self, domain, blocked, q_type):
        # Define the forwarding decision for the domain based on if it is blocked or not
        decision = "DENY" if blocked else "ALLOW"

        # Open the log file
        with open(self.log_file, "a") as file:
            # Define a writer for the file
            write_file = writer(file)

            # Write the domain, response type, and decision to ALLOW or DENY the domain
            write_file.writerow([f"{domain} {q_type} {decision}"])

    def test(self):
        # # Define the DNS request, "rd=1" means recursion
        # dns_req = DNS(rd=1, qd=DNSQR(qname='www.example.com'))

        # # Convert the request to binary
        # b = bytes(dns_req)

        # # Redefine the DNS request
        # p = DNS(b)

        domain = "eaxlpe.com"

        # # Show the DNS request
        # p.show()

        dns_request = IP(dst="9.9.9.9") / UDP(dport=53) / DNS(rd=1, qd=DNSQR(qname=domain))
        response = sr1(dns_request, verbose=0, timeout=2)

        response.show()

        # r = requests.get(f"https://{domain}")
        # print(r.status_code)
        # print(r)


"""
NXDOMAIN Example
-----------------------------
; <<>> DiG 9.18.39-0ubuntu0.24.04.1-Ubuntu <<>> @9.9.9.9 -t A www.eaxpmel.com
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NXDOMAIN, id: 35601
;; flags: qr rd ra; QUERY: 1, ANSWER: 0, AUTHORITY: 1, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 1232
;; QUESTION SECTION:
;www.eaxpmel.com.		IN	A

;; AUTHORITY SECTION:
com.			895	IN	SOA	a.gtld-servers.net. nstld.verisign-grs.com. 1760103674 1800 900 604800 900

;; Query time: 16 msec
;; SERVER: 9.9.9.9#53(9.9.9.9) (UDP)
;; WHEN: Fri Oct 10 09:41:39 EDT 2025
;; MSG SIZE  rcvd: 117
"""

if __name__ == "__main__":
    main()
