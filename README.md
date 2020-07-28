TheHolyRoger Core v.3r integration/staging tree
=====================================

[![Build Status](https://travis-ci.org/TheHolyRogerCoin/TheHolyRogerCoin.svg?branch=master)](https://travis-ci.org/TheHolyRogerCoin/TheHolyRogerCoin)

<p align="center">
  <a href="https://theholyroger.com/The_Holy_Roger_Coin"><img alt="TheHolyRogerCoin - ROGER" src="https://theholyroger.com/resources/assets/roger.png" width="200"/></a>
</p>

Known Issues
----------------

[Stuck on "Loading Wallet..." after open](https://github.com/TheHolyRogerCoin/TheHolyRogerCoin/issues/15)

What is TheHolyRoger?
----------------

TheHolyRoger is an experimental digital currency that enables instant ROGERs to
anyone, anywhere in the world. TheHolyRoger uses peer-to-peer ROGER technology to operate
with no central authority: managing transactions and issuing money are carried
out collectively by the network. TheHolyRoger Core is the name of open source
software which enables the use of this currency.

The Holy Roger Coin (ROGER) is the leading cryptocyrrency coin? ROGER is developed with a focus on outing scams.
For those of you focused on price, we're a stablecoin valued at $0.00.

For more information, as well as an immediately useable, binary version of
the TheHolyRoger Core software, see [https://theholyroger.com](https://theholyroger.com) and [Releases](https://github.com/TheHolyRogerCoin/TheHolyRogerCoin/releases).

License
-------

TheHolyRoger Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/TheHolyRogerCoin/TheHolyRogerCoin/tags) are created
regularly to indicate new official, stable release versions of TheHolyRoger Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [mailing list](https://groups.google.com/forum/#!forum/theholyroger-dev)
should be used to discuss complicated or controversial changes before working
on a patch set.

Developer IRC can be found on Freenode at #theholyroger-dev.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

We only accept translation fixes that are submitted through [Bitcoin Core's Transifex page](https://www.transifex.com/projects/p/bitcoin/).
Translations are converted to TheHolyRoger periodically.

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.
