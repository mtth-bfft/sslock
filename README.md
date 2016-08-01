# sslock
A minimalist PAM-enabled screenlock with no GUI, in a couple hundred lines of C.

The goal of this project is to create a secure screenlock with no GUI (in particular,
it does not overlay anything to hide what is on your screen), with as few SLOCs as possible
and the possibility to hook every event and customize at will.

**/!\ This is still a beta project, use at your own risk.**

## Dependencies
 * libX11 to catch user inputs;
 * libpam to authenticate.

## Build and run
To compile it, you need the libpam and libX11 libraries along with their development headers
(libpam0g & libpam0g-dev & libx11-6 & libx11-dev in aptitude, or pam & pam-devel & libX11 & libX11-devel in DNF) and then:

    git clone https://github.com/mtth-bfft/sslock.git
    make
    ./sslock

## Contribute
If you use it and find it useful, and if you find bugs or have suggestions to improve this, I'd love to hear about it.
Send me a PR or drop me an email at mtth.bfft at gmail.
