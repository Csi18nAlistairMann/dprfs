# dprfs
Data-poisoning resistant filesystem

Ransomware as a problem would be solved with a filesystem that refuses to allow *anything* to be overwritten or 
deleted, even as it appears to behave normally. DPRFS is that filesystem.

To see it in action, 
DPRFS vs Ransomware: https://www.youtube.com/watch?v=VsBAKeOQh78

DPRFS vs mass deletes by a rogue user: https://www.youtube.com/watch?v=BeTdFSlHMJY

The whitepaper can be read at https://github.com/Csi18nAlistairMann/dprfs/blob/master/docs/Ransomwareproof%20-%20a%20prototype.pdf

This project is written in C. https://github.com/Csi18nAlistairMann/dprfs/blob/master/src/dprfs.c for the main 
file.

This 2015 project of mine developed from seeing too many ransomware infections in the few years leading up to it.
The essential observation is to store files as a linked list, with changes resulting in a new link at the end of
the given list. That change might be the addition of a "deleted" flag, for instance. When accessed by the user,
however, they can only see the end of each list - and so the file system appears to be otherwise normal.

The project requires Linux and FUSE - everything else is standard networking. The code remains in live use in a
small number of installations.
