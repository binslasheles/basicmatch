###############################################################################
##
##  Copyright (C) 2013 Tavendo GmbH
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
###############################################################################

from twisted.internet.protocol import Protocol, Factory
from autobahn.twisted.websocket import WebSocketClientProtocol, WebSocketClientFactory
import traceback

class Websockify(WebSocketClientProtocol):
    def onOpen(self):
        #print "======= FINALLY =========="
        WebSocketClientProtocol.onOpen(self)
        self.tcpproxy.set_wsproxy(self)
        #self.sendMessage(u"Hello, world!".encode('utf8'))

    def onClose(self, clean, code, why):
        ref = self.tcpproxy
        self.tcpproxy = None
        ref.set_wsproxy(None)
        ref.transport.loseConnection()

    def onMessage(self, payload, is_binary):
        WebSocketClientProtocol.onMessage(self, payload, is_binary)
        self.tcpproxy.transport.write(payload)
        #print(" ===== received {} bytes {}".format(len(payload), payload))
    #will need to 

    #add disconnect handler

class WebsockifyClientFactory(WebSocketClientFactory):
    protocol = Websockify

    def __init__(self, tcpproxy, *args, **kwargs):
        WebSocketClientFactory.__init__(self, *args, **kwargs)
        self.tcpproxy = tcpproxy #tcp 

    def buildProtocol(self, *args, **kwargs):
        ret = WebSocketClientFactory.buildProtocol(self, *args, **kwargs) #websockify obj
        ret.tcpproxy = self.tcpproxy
        return ret

class Fwd(Protocol):
    def connectionMade(self):
        #print "======== connection made ========"
        self.data_queue = [] 
        self.set_wsproxy(None)
        reactor.connectTCP("192.168.1.104", 9090, WebsockifyClientFactory(self, protocols=['wamp.2.msgpack.batched','wamp.2.msgpack']))

    def set_wsproxy(self, wsproxy):
        #print '== SETTING PROXY ==='
        self.wsproxy = wsproxy

        if(self.wsproxy is not None):
            for msg in self.data_queue:
                self.wsproxy.sendMessage(msg, True)

            del self.data_queue[:]

    def connectionLost(self, why):
        #print 'disconnect'
        if(self.wsproxy is not None):
            ref = self.wsproxy  
            self.wsproxy = None
            ref.sendClose()

    def dataReceived(self, data):
        if( self.wsproxy is None ):
            #print 'queuing data'
            self.data_queue.append(data)
        else:
            #print 'sending data'
            self.wsproxy.sendMessage(data, True)

        #print("bytes {} dataReceived: {}".format(len(data), data))

if __name__ == '__main__':

   import sys

   from twisted.python import log
   from twisted.internet import reactor
   from twisted.internet.protocol import Factory
   from twisted.internet.endpoints import serverFromString

   log.startLogging(sys.stdout)
   f = Factory()
   f.protocol = Fwd
   reactor.listenTCP(9091, f, interface='192.168.1.104')

   #wrappedFactory = Factory.forProtocol(HelloServerProtocol)

   #endpoint = serverFromString(reactor, "autobahn:tcp\:9000:url=ws\://localhost\:9000")
   #endpoint = serverFromString(reactor, "autobahn:tcp\:9091:url=ws\://192.168.1.104\:9090")
   #endpoint.listen(wrappedFactory)

   reactor.run()
