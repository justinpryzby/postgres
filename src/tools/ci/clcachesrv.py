# We often don't use all members of all the pyuv callbacks
# pylint: disable=unused-argument
import hashlib
import logging
import os
import pickle
#import signal
import argparse
import re

import asyncio
import socket
import sys

class HashCache:
    def __init__(self, excludePatterns, disableWatching):
        self._watchedDirectories = {}
        self._handlers = []
        self._excludePatterns = excludePatterns or []
        self._disableWatching = disableWatching

    def getFileHash(self, path):
        logging.debug("getting hash for %s", path)
        dirname, basename = os.path.split(os.path.normcase(path))

        watchedDirectory = self._watchedDirectories.get(dirname, {})
        hashsum = watchedDirectory.get(basename)
        if hashsum:
            logging.debug("using cached hashsum %s", hashsum)
            return hashsum

        with open(path, 'rb') as f:
            hashsum = hashlib.md5(f.read()).hexdigest()

        watchedDirectory[basename] = hashsum
        if dirname not in self._watchedDirectories and not self.isExcluded(dirname) and not self._disableWatching:
            logging.debug("starting to watch directory %s for changes", dirname)
            self._startWatching(dirname)

        self._watchedDirectories[dirname] = watchedDirectory

        logging.debug("calculated and stored hashsum %s", hashsum)
        return hashsum

    #def _startWatching(self, dirname):
        #ev = pyuv.fs.FSEvent(self._loop)
        #ev.start(dirname, 0, self._onPathChange)
        #self._handlers.append(ev)

    #def _onPathChange(self, handle, filename, events, error):
        #watchedDirectory = self._watchedDirectories[handle.path]
        #logging.debug("detected modifications in %s", handle.path)
        #if filename in watchedDirectory:
            #logging.debug("invalidating cached hashsum for %s", os.path.join(handle.path, filename))
            #del watchedDirectory[filename]

    def __del__(self):
        for ev in self._handlers:
            ev.stop()

    def isExcluded(self, dirname):
        # as long as we do not have more than _MAXCACHE regex we can
        # rely on the internal cacheing of re.match
        excluded = any(re.search(pattern, dirname, re.IGNORECASE) for pattern in self._excludePatterns)
        if excluded:
            logging.debug("NOT watching %s", dirname)
        return excluded


async def handle_read(reader, writer):
    #print('reader', type(reader), dir(reader), file=sys.stderr)
    readBuffer = await reader.read(8 * 1024)
    while readBuffer:
        if readBuffer.endswith(b'\x00'):
            paths = readBuffer[:-1].decode('utf-8').splitlines()
            logging.debug("received request to hash %d paths", len(paths))
            try:
                hashes = map(CACHE.getFileHash, paths)
                response = '\n'.join(hashes).encode('utf-8')
            except OSError as e:
                response = b'!' + pickle.dumps(e)
            writer.write(response + b'\x00')
            writer.close()
            return

        readBuffer += await reader.read(8 * 1024)

    #writer.drain()
    #writer.write_eof()


class PipeServer():
    def __init__(self, address, cache):
        self.create_socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.bind(address)

        self._connections = []
        self._cache = cache

    def handle_accept(self):
        logging.debug("detected incoming connection")
        pair = self.accept()
        if pair is None:
            print('pair was none', file=sys.stderr)
            return
        #pair is (sock, addr)
        client = pair[0]
        #self._connections.append(Connection(client, self._cache, self._connections.remove))


def closeHandlers(handle):
    for h in handle.loop.handles:
        h.close()


def onSigint(handle, signum):
    logging.info("Ctrl+C detected, shutting down")
    closeHandlers(handle)


def onSigterm(handle, signum):
    logging.info("Server was killed by SIGTERM")
    closeHandlers(handle)

# https://gist.github.com/AndreLouisCaron/842178ef3c7adc3c6460f4872ea279cf
DEFAULT_LIMIT = 2 ** 16
async def start_pipe_server(client_connected_cb, *, path,
                            loop=None, limit=DEFAULT_LIMIT):
    """Start listening for connection using Win32 named pipes."""

    loop = loop or asyncio.get_event_loop()

    def factory():
        reader = asyncio.StreamReader(limit=limit, loop=loop)
        protocol = asyncio.StreamReaderProtocol(
            reader, client_connected_cb, loop=loop
        )
        return protocol

    # NOTE: has no "wait_closed()" coroutine method.
    server, *_ = await loop.start_serving_pipe(factory, address=path)
    return server
#
async def serve_until(cancel, *, path, session, loop=None, ready=None):
    """IPC server."""

    loop = loop or asyncio.get_event_loop()
    sessions = set()

    def client_connected(reader, writer):
        sessions.add(loop.create_task(session(reader, writer)))

    # Start accepting connections.
    print('S: prepping server...')
    global server
    server = await asyncio.wait_for(
        start_pipe_server(client_connected, path=path),
        timeout=5.0
    )

    try:
        # Let the caller know we're ready.
        print('S: signalling caller...')
        ready.set_result(None)

        # Serve clients until we're told otherwise.
        print('S: serving...')
        await cancel
    finally:
        # Stop accepting connections.
        print('S: closing...')
        server.close()
        if hasattr(server, 'wait_closed'):
            await server.wait_closed()

        # Wait for all sessions to complete.
        print('S: waiting on sessions...')
        for session in asyncio.as_completed(sessions):
            await session


async def xmain(path):
    cancel, ready = asyncio.Future() , asyncio.Future()
    loop = asyncio.get_event_loop()
    _server = serve_until(cancel, path=path, session=handle_read, ready=ready)
    server = loop.create_task(_server)
    try:
        # Wait until the server is ready.
        print('C: waiting for server to boot...')
        await ready
    finally:
        # Stop accepting connections.
        print('C: stopping server...')
        cancel.set_result(None)
        print('C: waiting for server to shutdown...')
        await server

def main():
    logging.basicConfig(format='%(asctime)s [%(levelname)s]: %(message)s', level=logging.INFO)

    parser = argparse.ArgumentParser(description='Server process for clcache to cache hash values of headers \
                                                  and observe them for changes.')
    parser.add_argument('--exclude', metavar='REGEX', action='append', \
                        help='Regex ( re.search() ) for excluding of directory watching. Can be specified \
                              multiple times. Example: --exclude \\\\build\\\\')
    parser.add_argument('--disable_watching', action='store_true', help='Disable watching of directories which \
                         we have in the cache.')
    args = parser.parse_args()

    for pattern in args.exclude or []:
        logging.info("Not watching paths which match: %s", pattern)

    if args.disable_watching:
        logging.info("Disabled directory watching")

    global CACHE
    CACHE = HashCache(vars(args)['exclude'], True)

    #address = ('', 12345)
    #server = PipeServer(path, cache)

    logging.info("clcachesrv started")

    path = r'\\.\pipe\clcache_srv'
    asyncio.set_event_loop(asyncio.ProactorEventLoop())
    loop = asyncio.get_event_loop()
    cancel, ready = asyncio.Future() , asyncio.Future()
    _server = serve_until(cancel, path=path, session=handle_read, ready=ready)
    server = loop.create_task(_server)
    loop.run_forever()

if __name__ == '__main__':
    main()
