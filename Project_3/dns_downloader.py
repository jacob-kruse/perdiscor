from argparse import ArgumentParser


def main():
    # Define arguments
    desc = "A DNS forwarder with domain blocking and DoH capabilities"
    parser = ArgumentParser(description=desc)
    parser.add_argument("-d", dest="dst_ip", default="192.1.1.1", help="Destination DNS server IP")
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
    print(f"DoH Server: {doh_server}")


if __name__ == "__main__":
    main()
