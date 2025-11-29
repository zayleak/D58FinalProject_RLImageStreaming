#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import Controller, OVSKernelSwitch
from mininet.log import setLogLevel, info
from mininet.link import TCLink
import time
import os

# --- CONFIGURATION ---
CLIENT_PORT = 12345
SERVER_IP = '10.0.0.2'
IMAGE_FILE = 'test_image.jpg'
CLIENT_LOG_FILE = 'h2_client_log.txt'
CLIENT_SAVE_FILE = 'received_frame_0000.jpg'
CLIENT_TIMEOUT_SEC = 6  # Should be > C client's timeout (5s)
TEST_DURATION_SEC = 20  # How long the server will stream
# ---------------------

class LossyRTPTopo:
    """A minimal two-host topology with a lossy link."""
    
    def setup_network(self):
        info('*** Starting Mininet with TCLink\n')
        net = Mininet(topo=None, build=False)

        info('*** Adding controller and switch\n')
        c0 = net.addController('c0', controller=Controller, protocol='tcp', port=6633)
        s1 = net.addSwitch('s1', cls=OVSKernelSwitch)

        info('*** Add hosts\n')
        h1 = net.addHost('h1', ip='10.0.0.1/24') # Server
        h2 = net.addHost('h2', ip='10.0.0.2/24') # Client

        info('*** Add links with impairments (5.0%% loss, 10ms delay)\n')
        net.addLink(h1, s1, cls=TCLink, bw=100)
        net.addLink(h2, s1, 
                    cls=TCLink,
                    bw=10, 
                    delay='10ms', 
                    loss=5.0) # 5.0% packet loss for testing UDP

        net.build()
        c0.start()
        s1.start([c0])
        return net, h1, h2

def run_test():
    setLogLevel('info')
    
    net, h1, h2 = LossyRTPTopo().setup_network()
    
    # 1. Clear any old log/image files from the host OS
    info('*** Cleaning up previous output files from /tmp\n')
    if os.path.exists('/tmp/' + CLIENT_LOG_FILE):
        os.remove('/tmp/' + CLIENT_LOG_FILE)
    if os.path.exists('/tmp/' + CLIENT_SAVE_FILE):
        os.remove('/tmp/' + CLIENT_SAVE_FILE)

    try:
        # --- A. Start Client (h2) in Background with Redirection ---
        # The client will write its output (stats, warnings) to the log file.
        info('*** Starting Client (h2) in background...\n')
        client_cmd = './client {} > {} 2>&1 &'.format(CLIENT_PORT, CLIENT_LOG_FILE)
        h2.cmd(client_cmd)
        
        # Give the client a moment to initialize the socket
        time.sleep(1) 

        # --- B. Start Server (h1) in Foreground ---
        # The server runs until its duration is reached or Ctrl+C is pressed.
        info('*** Starting Server (h1) for {} seconds...\n'.format(TEST_DURATION_SEC))
        server_cmd = './server {} {} {} &'.format(SERVER_IP, CLIENT_PORT, IMAGE_FILE)
        
        # We start the server in the background, but then use a pause to let it run
        server_process_info = h1.cmd(server_cmd)
        
        # Wait for the test duration
        info('*** Streaming active. Waiting for {} seconds...\n'.format(TEST_DURATION_SEC))
        time.sleep(TEST_DURATION_SEC)
        
        # --- C. Stop Server ---
        info('*** Stopping server process on h1.\n')
        h1.cmd('kill %server') # Kill the background server job
        
        # Wait for the client to hit its timeout and write final stats
        info('*** Waiting {}s for client timeout and final stats...\n'.format(CLIENT_TIMEOUT_SEC))
        time.sleep(CLIENT_TIMEOUT_SEC)
        
        # --- D. Retrieve Files ---
        info('*** Retrieving log and saved frame from h2 to /tmp/ ...\n')
        
        # Copy the log file
        h2.cmd('cp {} /tmp/'.format(CLIENT_LOG_FILE))
        
        # Copy the saved frame file
        h2.cmd('cp {} /tmp/'.format(CLIENT_SAVE_FILE))
        
        info('*** Client finished. Output files are in /tmp/ on the host OS:\n')
        info('    - Log: /tmp/{}\n'.format(CLIENT_LOG_FILE))
        info('    - Image: /tmp/{}\n'.format(CLIENT_SAVE_FILE))

    except Exception as e:
        print("An error occurred during the test:", e)
    
    finally:
        info('*** Stopping network\n')
        net.stop()

if __name__ == '__main__':
    # Ensure you are running this from the directory containing 
    # ./client, ./server, and test_image.jpg
    run_test()