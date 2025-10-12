#!/usr/bin/env python3

"""
This script acts as DNS Forwarder that blocks requests to specified domains
"""

from requests import get
from csv import reader, writer
from argparse import ArgumentParser
from base64 import urlsafe_b64encode
from scapy.layers.dns import DNS, DNSQR, dnsqtypes
from socket import socket, AF_INET, SOCK_DGRAM, SOCK_STREAM


def main():
    # Define arguments
    desc = "A DNS forwarder with domain blocking and DoH capabilities"
    parser = ArgumentParser(description=desc)
    parser.add_argument("-d", dest="dst_ip", default="1.1.1.1", help="Destination DNS server IP")
    parser.add_argument("-f", dest="deny_list_file", default="deny_list.csv", help="File containing domains to block")
    parser.add_argument("-l", dest="log_file", default="log.csv", help="Append-only log file")
    parser.add_argument("--doh", dest="doh", action="store_true", help="Use default upstream DoH server")
    parser.add_argument("--doh_server", dest="doh_server", default="8.8.8.8", help="Use this upstream DoH server")

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

        # Determine the DNS forwarding socket type based on whether or not DoH is desired
        socket_type = SOCK_STREAM if doh else SOCK_DGRAM

        # Create sockets, AF_INET=IPv4, SOCK_DGRAM=UDP
        temp_sock = socket(AF_INET, SOCK_DGRAM)
        self.req_sock = socket(AF_INET, SOCK_DGRAM)
        self.dns_sock = socket(AF_INET, socket_type)

        # Connect to the temporary socket, get the socket name to extract the device's IP, and close the socket
        temp_sock.connect(("8.8.8.8", 80))
        local_ip = temp_sock.getsockname()[0]
        temp_sock.close()

        # Bind the request socket to this device's IP and port 53 (Port for DNS requests)
        self.req_sock.bind((local_ip, 53))

    def listen(self):
        # Start loop
        while True:
            # Listen for DNS requests to the server on the request socket
            request, address = self.req_sock.recvfrom(512)

            # Pass the request and client address to the handle_request() function
            self.handle_request(request, address)

    def handle_request(self, request, address):
        # Transform the raw bytes into a readable DNS message, then get the domain and query type from the request
        dns_request = DNS(request)
        domain = dns_request[DNSQR].qname.decode().rstrip(".")
        qt = dns_request[DNSQR].qtype
        q_type = dnsqtypes.get(qt, str(qt))

        # Check if the domain is blocked
        blocked = self.check_domain(domain)

        # If the domain is blocked, return an NXDomain response
        if blocked:
            # Construct the NXDomain response based on the request, then convert it to bytes
            # Response: qr=1, Recursion Available: ra=1, NXDomain: rcode=3, No Additional Records: ar=None
            dns_response = DNS(request, qr=1, ra=1, ad=0, rcode=3, arcount=0, ar=None)
            response = bytes(dns_response)

        # If the domain is not blocked, forward the request to the desired DNS resolver
        elif not blocked:
            # If DoH requests are desired
            if self.doh:
                # Convert the request from bytes -> Base64, send the GET request to the DoH server, and extract the content
                b64_request = urlsafe_b64encode(request).rstrip(b"=").decode()
                doh_response = get(f"https://{self.doh_server}/dns-query?dns={b64_request}")
                response = doh_response.content

            # If normal DNS request over UDP are desired
            elif not self.doh:
                # Send the DNS request to the upstream DNS server, and listen for a response
                self.dns_sock.sendto(request, (self.dst_ip, 53))
                response, _ = self.dns_sock.recvfrom(512)

        # Log whether or not the domain was blocked and the query type
        self.log_domain(domain, blocked, q_type)

        # Forward the DNS response back to the client
        self.req_sock.sendto(response, address)

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
        pass


if __name__ == "__main__":
    main()
