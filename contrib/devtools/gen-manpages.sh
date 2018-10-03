#!/bin/bash

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

THEHOLYROGERD=${THEHOLYROGERD:-$SRCDIR/theholyrogerd}
THEHOLYROGERCLI=${THEHOLYROGERCLI:-$SRCDIR/theholyroger-cli}
THEHOLYROGERTX=${THEHOLYROGERTX:-$SRCDIR/theholyroger-tx}
THEHOLYROGERQT=${THEHOLYROGERQT:-$SRCDIR/qt/theholyroger-qt}

[ ! -x $THEHOLYROGERD ] && echo "$THEHOLYROGERD not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
ROGERVER=($($THEHOLYROGERCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for bitcoind if --version-string is not set,
# but has different outcomes for bitcoin-qt and bitcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$THEHOLYROGERD --version | sed -n '1!p' >> footer.h2m

for cmd in $THEHOLYROGERD $THEHOLYROGERCLI $THEHOLYROGERTX $THEHOLYROGERQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${ROGERVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${ROGERVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
