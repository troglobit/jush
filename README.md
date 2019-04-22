jush - just a useless shell
===========================
[![Travis Status][]][Travis]

jush is an almost useless shell for UNIX systems.  It is developed on
Ubuntu and Debian GNU/Linux but is regularly tested on FreeBSD as well.

![The jush shell](jush.png)
> _"I'm writing my own shell, it's nothing fancy like bash or anything ..."_


Goals
-----

- Have fun
- Be an example of [editline][] use


Targets
-------

- [x] Pipes
- [x] Backgrounding and job control
- [x] Redirect
- [x] Environment variables
- [x] Command completion
- [x] Filename completion


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

[Travis]:        https://travis-ci.com/troglobit/jush
[Travis Status]: https://travis-ci.com/troglobit/jush.png?branch=master
[editline]:      https://github.com/troglobit/editline
