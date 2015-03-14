# HTTrack Website Copier - Development Repository #

## About ##
_Copy websites to your computer (Offline browser)_

![http://www.httrack.com/images/screenshot_01.jpg](http://www.httrack.com/images/screenshot_01.jpg)
<img src='http://www.httrack.com/htsw/screenshot_w1.jpg' width='34%'>

<b>HTTrack</b> is an <i>offline browser</i> utility, allowing you to download a World Wide website from the Internet to a local directory, building recursively all directories, getting html, images, and other files from the server to your computer.<br>
<br>
<b>HTTrack</b> arranges the original site's relative link-structure. Simply open a page of the "mirrored" website in your browser, and you can browse the site from link to link, as if you were viewing it online.<br>
<br>
HTTrack can also update an existing mirrored site, and resume interrupted downloads. HTTrack is fully configurable, and has an integrated help system.<br>
<br>
<b>WinHTTrack</b> is the Windows 2000/XP/Vista/Seven release of HTTrack, and <b>WebHTTrack</b> the Linux/Unix/BSD release.<br>
<br>
<h2>Website</h2>

<b>Main Website:</b>
<a href='http://www.httrack.com/'>http://www.httrack.com/</a>

<h2>Compile trunk release</h2>
<pre><code>svn checkout https://httrack.googlecode.com/svn/trunk/ httrack<br>
cd httrack<br>
./configure --prefix=$HOME/usr &amp;&amp; make -j8 &amp;&amp; make install<br>
</code></pre>