jush - just a useless shell
===========================
[![License Badge][]][License] [![Travis Status][]][Travis] [![Coverity Status][]][Coverity Scan]

jush is an almost useless shell for UNIX systems.  It is developed on
Ubuntu and Debian GNU/Linux but is regularly tested on FreeBSD as well.

![The jush shell](jush.png)
> _"I'm writing my own shell, it's nothing fancy like bash or anything ..."_


Features
--------

- Pipes using `|`
- Backgrounding using `&`
- Basic job control
- Redirect, basic using `<` and `>`
- Command separation using `;`
- Conditional execution `cmd && cmd` and `cmd || cmd`
- Environment variables (basic)
- Command completion
- Filename completion


Building
--------

The project use the de facto standard GNU configure and build system to
ensure portability, so all you need is to run the `./configure` script
to get up and running.

**Requirement:** [editline][]

If you check out the sources from GIT, first run `./autogen.sh` which
creates the necessary files -- this also requires the GNU autoconf and
automake tools.


Origin & References
-------------------

jush was written from scratch by [Joachim Nilsson](http://troglobit.com)
and is availalbe as Open Source under the ISC license.

The name doesn't really mean anything, but it could be one of:

- Jocke's UNIX SHell
- Just give me a UNIX SHell
- Just a Useless SHell
- Jocke's ush (micro shell)

> The short form of Joachim is Jocke, only makes sense in Swedish

[Travis]:          https://travis-ci.org/troglobit/jush
[Travis Status]:   https://travis-ci.org/troglobit/jush.png?branch=master
[License]:         https://en.wikipedia.org/wiki/ISC_license
[License Badge]:   https://img.shields.io/badge/License-ISC-blue.svg
[Coverity Scan]:   https://scan.coverity.com/projects/18269
[Coverity Status]: https://scan.coverity.com/projects/18269/badge.svg
[editline]:        https://github.com/troglobit/editline
