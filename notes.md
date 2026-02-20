ssh -vvv -i ~/.ssh/hetzner01 deploy@157.90.114.127
pass: Oxekopp123!


# 42_IRC Deployment Guide

This document provides a step-by-step guide to deploying the 42_IRC server on a VPS. It covers installing dependencies, uploading the project, building the server, configuring the firewall, and setting up a systemd service for easy management.

todo:

done - implementieren /kick --> Matthias 
done - implementieren /topic --> Matthias
done - topic neu setzen nur, wenn der channel das erlaubt (mode -t --> sonst nur operatoren erlaubt) --> Matthias
almost done - implementieren /mode --> Daniel
done - sanitzing channel namen - &/# sind als erstes Zeichen Bedingung --> Matthias
done - /JOIN erweitern? Befehl auf mehrere argumente erweitern? /join #channel1,#channel2,#channel3 --> Matthias
almost all done - /MODE string plus parameters? mode k --> siehe nächster punkt
done - /KEY pro channel?
done - Livetest mit allen 1h
done - channel limit? mode +l --> Daniel
done - mode +o funktioniert nicht? Operatorrechte wegnehmen ist nun das Thema, nachträglich verliehene werden auch noch nicht überall erkannt
almost done - netcat behaviour!!! --> Lewin, vielleicht 

- mehrere flags zugleich? bsp: /mode #channel +nt --> Daniel
- resources angeben!! auch KI Nutzung - Abgleich mit Protokollverhalten... --> Matthias, Daniel, Lewin
- server debug session starten, nochmal mit valgrind... --> Lewin
- graceful shutdown verbessern? --> Lewin
- implementieren: invite --> Daniel
- shell-script zum neustarten des servers --> Matthias






