#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import Controller, OVSKernelSwitch
from mininet.log import setLogLevel, info
from mininet.link import TCLink
import time
import os
import argparse

# --- DEFAULT CONFIGURATION ---
CLIENT_PORT = 12345
SERVER_IP = '10.0.0.2'
IMAGE_FILE = 'test_image.jpg'
CLIENT_LOG_FILE = 'h2_client_log.txt'
CLIENT_TIMEOUT_SEC = 6  # Should be > C client's timeout (5s)
# ---------------------

def setup_network(loss, delay, bw, reorder):
    info('*** Starting Mininet with TCLink (Loss: {}%, Delay: {}, BW: {}Mbps, Reorder: {}%)\n'.format(loss, delay, bw, reorder))
    net = Mininet(topo=None, build=False)

    info('*** Adding controller and switch\n')
    c0 = net.addController('c0', controller=Controller, protocol='tcp', port=6633)
    s1 = net.addSwitch('s1', cls=OVSKernelSwitch)

    info('*** Add hosts\n')
    h1 = net.addHost('h1', ip='10.0.0.1/24') 
    h2 = net.addHost('h2', ip='{}/24'.format(SERVER_IP)) 

    info('*** Add links with impairments\n')
    net.addLink(h1, s1, cls=TCLink, bw=100) 
    net.addLink(h2, s1, 
                cls=TCLink, 
                bw=bw, 
                delay=delay, 
                loss=loss,
                reordering=reorder)

    net.build()
    c0.start()
    s1.start([c0])
    return net, h1, h2

def run_test_scenario(duration_sec, loss, delay, bw, reorder):
    net, h1, h2 = setup_network(loss, delay, bw, reorder)
    log_path = '/tmp/{}'.format(CLIENT_LOG_FILE) 
    if os.path.exists(log_path):
        os.remove(log_path)

    try:
        info('*** Starting Client (h2) in background...\n')

        client_cmd = './client {} > {} 2>&1 &'.format(CLIENT_PORT, CLIENT_LOG_FILE)
        h2.cmd(client_cmd)
        
        time.sleep(1) 

        info('*** Starting Server (h1) for {} seconds...\n'.format(duration_sec))

        server_cmd = './server {} {} {} &'.format(SERVER_IP, CLIENT_PORT, IMAGE_FILE)
        
        h1.cmd(server_cmd)
        

        info('*** Streaming active. Waiting for {} seconds...\n'.format(duration_sec))
        time.sleep(duration_sec)
        

        info('*** Stopping server process on h1.\n')
        h1.cmd('kill %server') 
        info('*** Waiting {}s for client timeout and final stats...\n'.format(CLIENT_TIMEOUT_SEC))
        time.sleep(CLIENT_TIMEOUT_SEC)
        
        info('*** Retrieving log and saved frame from h2 to /tmp/ ...\n')
        h2.cmd('cp {} /tmp/'.format(CLIENT_LOG_FILE))

        info('*** Test finished. Output files are in /tmp/ on the host OS:\n')
        info('    - Log: {}\n'.format(log_path))

    except Exception as e:
        print("An error occurred during the test: {}".format(e))
        
    finally:
        info('*** Stopping network\n')
        net.stop()


def simple_topo_only(loss, delay, bw, reorder):
    net, h1, h2 = setup_network(loss, delay, bw, reorder)
    
    info('*** Topology built. Dropping into CLI for manual testing.\n')
    info('*** Use \'h1 ping h2\' or manually run ./server and ./client.\n')
    net.cli()
    net.stop()


def main():
    parser = argparse.ArgumentParser(description='Mininet RTP Streaming Tester.')
    parser.add_argument('--test', type=str, default='full', choices=['full', 'cli', 'lossy'],
                        help='Test to run: "full" (default), "cli" (topology only), or "lossy" (pre-defined lossy test).')
    parser.add_argument('--duration', type=int, default=20,
                        help='Duration of the stream in seconds (default: 20).')
    parser.add_argument('--loss', type=float, default=0.0,
                        help='Packet loss rate %% for the client link (default: 0.0).')
    parser.add_argument('--delay', type=str, default='0ms',
                        help='Delay for the client link (e.g., "50ms") (default: "0ms").')
    parser.add_argument('--bw', type=int, default=10,
                        help='Bandwidth in Mbps for the client link (default: 10).')
    parser.add_argument('--reorder', type=float, default=0.0,
                        help='Packet reordering probability %% for the client link (default: 0.0).')

    args = parser.parse_args()
    setLogLevel('info')

    if args.test == 'cli':
        simple_topo_only(args.loss, args.delay, args.bw, args.reorder)
    elif args.test == 'lossy':
        info('*** Running PRE-DEFINED LOSS TEST (5.0%% loss, 10ms delay, 10Mbps BW, 0.0%% reorder)\n')
        run_test_scenario(20, 5.0, '10ms', 10, 0.0)
    else:
        info('*** Running FULL CUSTOM TEST (Duration: {}s)\n'.format(args.duration))
        run_test_scenario(args.duration, args.loss, args.delay, args.bw, args.reorder)


if __name__ == '__main__':
    main()