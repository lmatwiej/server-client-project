#!/usr/bin/env python3

import concurrent.futures
import os
import requests
import sys
import time

# Functions

def usage(status=0):
    progname = os.path.basename(sys.argv[0])
    print(f'''Usage: {progname} [-h HAMMERS -t THROWS] URL
    -h  HAMMERS     Number of hammers to utilize (1)
    -t  THROWS      Number of throws per hammer  (1)
    -v              Display verbose output
    ''')
    sys.exit(status)

def hammer(url, throws, verbose, hid):
    ''' Hammer specified url by making multiple throws (ie. HTTP requests).

    - url:      URL to request
    - throws:   How many times to make the request
    - verbose:  Whether or not to display the text of the response
    - hid:      Unique hammer identifier

    Return the average elapsed time of all the throws.
    '''
    total_time = 0
    for throw in range(throws):
        time1 = time.time()
        res = requests.get(url)
        time2 = time.time()
        try:
            res.raise_for_status()
        except Exception as exc:
            print(f"There was a problem: {exc}")
            exit(1)

        if verbose:
            print(res.text)

        throw_time = time2 - time1
        total_time += throw_time
        print(f"Hammer {hid}, Throw:    {throw}, Elapsed Time: {throw_time:.2f}")
    average_time = total_time / throws
    print(f"Hammer {hid}, AVERAGE    , Elapsed Time: {average_time:.2f}")

    return average_time

def do_hammer(args):
    ''' Use args tuple to call `hammer` '''
    return hammer(*args)

def main():
    hammers = 1
    throws  = 1
    verbose = False
    
    if (len(sys.argv) > 1):
        arguments = sys.argv[1:]
    else:
        usage(1)

    # Parse command line arguments
    while arguments and arguments[0].startswith('-'):
        argument = arguments.pop(0)
        if argument == '-h':
            hammers = int(arguments.pop(0))
        elif argument == '-t':
            throws = int(arguments.pop(0))
        elif argument == '-v':
            verbose = True;
        else:
            usage(1)

    if len(arguments) > 0:
        url = arguments.pop(0)
    else:
        usage(1)
    

    # Create pool of workers and perform throws
    args = ((url, throws, verbose, hid) for hid in range(hammers))
    with concurrent.futures.ProcessPoolExecutor(hammers) as executor:
        average_times = executor.map(do_hammer, args);

    total_average = 0
    for time in average_times:
        total_average += time

    total_average /= hammers
    print(f"TOTAL AVERAGE ELAPSED TIME: {total_average:.2f}")

    return 0

# Main execution

if __name__ == '__main__':
    main()

# vim: set sts=4 sw=4 ts=8 expandtab ft=python:
