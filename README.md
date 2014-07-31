xoip2
=====

xoip2 : a simple module for asterisk


Dial plan example 
--------------
[test-xoip-alarms-receiver]

exten = _X.,1,NoOp(test-xoip-alarms-receiver)

same => n,XoipAlarm("Test")

same => n,Playback(tt-monkeys)

same => n,Hangup()

#so far so good somewhere into dialplan

exten => 8118,1,Answer()

exten => 8118,2,Goto(test-xoip-alarms-receiver,${EXTEN},1)

Testing
------------
Edit on the master before merge into brLocalTestServer

Edit on the brTestPPJ

Edit on the master

Edit on the master 2

Edit on the brTestPPJ

Edit on the brTestPPJ from local to the master

Edit on the brTestPPJ from local to the master 1

Edit on the brTestPPJ from local to the master 2

