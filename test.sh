#!/bin/bash  

dd if=/dev/zero of=cache.img bs=200M count=1
mkfs.ext2 -m 0 -F cache.img
#s3fs ece566-project remote_dir
#s3fs slow-network-share remote_dir/ -o endpoint=ap-southeast-1 -o url="https://s3-ap-southeast-1.amazonaws.com" -o max_stat_cache_size=0 -o stat_cache_expire=0,use_cache="",del_cache
s3fs slow-network-share remote_dir/ -o endpoint=ap-southeast-1 -o url="https://s3-ap-southeast-1.amazonaws.com" -o max_stat_cache_size=0 

cd cache_dir
rm -rf *
cd ..
##s3fs slow-network-share remote_dir/ -o endpoint=ap-southeast-1 -o url="https://s3-ap-southeast-1.amazonaws.com"
#dd if=/dev/zero of=cloudfs.img bs=200M count=1                                                                                 
#mkfs.ext2 -m 0 -F cloudfs.img                                                                                                  
#sudo mount -o loop cloudfs.img cloudfs     




