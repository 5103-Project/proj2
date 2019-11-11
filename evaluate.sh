#!/bin/bash

for((nframes = 2; nframes <= 100; nframes++))
do
	./virtmem 100 $nframes rand sort
	./virtmem 100 $nframes fifo sort
	./virtmem 100 $nframes clock sort
done

for((nframes = 2; nframes <= 100; nframes++))
do
	./virtmem 100 $nframes rand focus
	./virtmem 100 $nframes fifo focus
	./virtmem 100 $nframes clock focus
done

for((nframes = 2; nframes <= 100; nframes++))
do
	./virtmem 100 $nframes rand scan
	./virtmem 100 $nframes fifo scan
	./virtmem 100 $nframes clock scan
done