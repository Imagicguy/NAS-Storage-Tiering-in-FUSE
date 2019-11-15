#!/bin/bash  

dd if=/dev/zero of=cache.img bs=10M count=1
mkfs.ext2 -m 0 -F cache.img
#dd if=/dev/zero of=cloudfs.img bs=200M count=1                                                                                 
#mkfs.ext2 -m 0 -F cloudfs.img                                                                                                  
#sudo mount -o loop cloudfs.img cloudfs                                                                                         





