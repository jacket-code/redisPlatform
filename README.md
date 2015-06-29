# redisPlatform
a rpc platform base on redis 3.0.2 stable version

using doc and design doc :
http://www.jacketzhong.com/?p=220

usage:
1. download the code
2. cd deps && chmod +x update-jemalloc.sh && ./update-jemalloc.sh 3.6.0
3. cd redisPlatform && chmod +x src/mkreleasehdr.sh && make
4. cd solib && make -f cpp_Makefile && chmod +x c_Makefile
5. cd src && ./redis-server ../redis.conf
6. start client: redis-cli -p 4379 -a jacketzhong
7. test: rso 0x12347 '{ "argv1": 11, "argv2": 25 }'


then you can play it


the result will be: (cpp so module)

127.0.0.1:4379> rso 0x12347 '{ "argv1": 11, "argv2": 25 }'
sum:36

127.0.0.1:4379> rso 0x12347 '{ "argv1": 11, "argv2": 25 }'
sum:36

127.0.0.1:4379> rso 0x12347 '{ "argv1": 11, "argv2": 35 }'
sum:46
