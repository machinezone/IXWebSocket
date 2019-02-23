```
# Start transfer server, which is just a broadcast server at this point
./ws transfer # running on port 8080.

# Start receiver first
./ws receive ws://localhost:8080 

# Then send a file. File will be received and written to disk by the receiver process
./ws send ws://localhost:8080 /file/to/path
```
