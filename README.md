# basicmatch
very much a WIP

limit only order matching engine w/ level based market data feed handler

md requires msgpack-c

web component requires autobahnjs, underscorejs, backbonejs

python component requires twisted, autobahnpy 


<pre>
 +------------+               +------+               +---------+         +----+
 | web_ladder | <-websocket-> | wamp | <-websocket-> | wsproxy | <-tcp-> | md | <-udp---+
 +------------+               +------+               +---------+         +----+         |
                                 ^                                        ^             |
      +------------+             |                                        |             |
      | web_ladder | <-websocket-+                                        |       +--------+         +----+
      +------------+             |                                        +-tcp-> | bmatch | <-tcp-> | cl |
                                 |                                                +--------+         +----+
     +------------+              |
     | web_ladder | <-websocket--+
     +------------+
 </pre>
