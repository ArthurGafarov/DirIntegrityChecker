# DirIntegrityChecker
Service controls your files checksum

# Usage
```
$ ./output/dir_checker -h
Program options:
  -h [ --help ]                    Show help
  -d [ --daemonize ]               daemonize
  -D [ --dir ] arg                 The directory to monitore, can be setted by 
                                   CRC_SCAN_DIRECTORY environment variable
  -T [ --worker_threads ] arg (=0) Number of worker threads used for crc check,
                                   0 - auto
  -Q [ --queue ] arg (=100000)     Size of files queue
  -P [ --period ] arg (=0)         Recalculating period, in seconds, can be 
                                   setted by CRC_SCAN_DIRECTORY_PERIOD 
                                   environment variable
```
