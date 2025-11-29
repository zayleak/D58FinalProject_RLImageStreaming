To run (example usage)

wget https://picsum.photos/640/480.jpg -O test_image.jpg 
make
./client 5004
In a (new terminal)
./server 127.0.0.1 5004 test_image.jpg
