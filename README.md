# HTTrack Website Copier - Development Repository

## About
_Copy websites to your computer (Offline browser)_

<img src="http://www.httrack.com/htsw/screenshot_w1.jpg" width="34%">

*HTTrack* is an _offline browser_ utility, allowing you to download a World Wide website from the Internet to a local directory, building recursively all directories, getting html, images, and other files from the server to your computer.
 
*HTTrack* arranges the original site's relative link-structure. Simply open a page of the "mirrored" website in your browser, and you can browse the site from link to link, as if you were viewing it online.

HTTrack can also update an existing mirrored site, and resume interrupted downloads. HTTrack is fully configurable, and has an integrated help system.

*WinHTTrack* is the Windows 2000/XP/Vista/Seven release of HTTrack, and *WebHTTrack* the Linux/Unix/BSD release. 

## Website

*Main Website:*
http://www.httrack.com/

## Compile trunk release
```sh
git clone https://github.com/xroche/httrack.git --recurse
cd httrack
./configure --prefix=$HOME/usr && make -j8 && make install
```
