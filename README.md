# dfsdump (DAOS filesystem dump)

Small tool that dumps content of a file inside a POSIX container to another file (or stdout).
The intended use case is in environments where mounting the DAOS container might not be possible or required.

## Important Note

This is just a small tool intended to become familiar with the DAOS and DFS API.

## Compilation

Just `make` it with a compiler supporting C++11.
Add `-std=c++11` or a higher version in case it is supported by default.

## Usage

```
ddump /path/in/container/source /non-container/destination
```

The source is inside a file inside POSIX-typed container. The container, pool, service replica, and system name
must be provided via environment variables (see below). For using ddump, the container is not required to be mounted.
Only the DAOS agent needs to be running. The destination is an arbitrary file.

### Environment Variables

The DAOS pool, container, service replica, and system name must be provided via the environment variables:

 * `DAOS_POOL` -- UUID of the pool
 * `DAOS_CONT` -- UUID of the container
 * `DAOS_SVC` -- comma-separated list
 * `DAOS_GROUP` -- name of the DAOS system/group

## Licence

MIT
