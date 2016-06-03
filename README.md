DartSync

Team: Justin Lee, Tianqi Li, Josh Kerber, Charlie Li

This is a decentralized file synchronization system inspired by Dropbox.

To run:

1. Run 'make'

2. cd into 'tracker' and run ./Dartsync_tracker

3. On another terminal instance, cd into 'peer'. Create a directory called 'dartsync_folder' and fill it with any files
that you want the system to serve. Then, from directory 'peer', run ./Dartsync_peer -n'. The -n is essential as it indicates to the tracker that
it is providing the initial file directory.

4. On another terminal instance, create a directory where you want to receive the system's files. FROM THIS DIRECTORY, run Dartsync_peer.
This will likely involve having to enter something like '../peer/Dartsync_peer'. This will create a directory where the running system's files
will download to.

At this point, any change (modification, deletion, addition) to the file system on one peer instance will sync to any other peer in the system.
Step 4 can be repeated any number of times to add peers to the system.



**Credit to the CS60 instructors for creating the code used to guard data sent back and forth over the TCP stream