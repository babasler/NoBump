# NoBump

Der Server ist derjenige der Daten (Befehle) empfängt.
Der Client gibt die Daten (Befehle) an den Server weiter.

Technisch ist es so, dass der BLE Server ein Service und in dem Service eine Charakteristic bereitstelt und der Client in diese Daten über Bluetooth schickt.

Der Server soll entsprechend reagieren und je nach Befehl, ein Warn Signal an, oder ausschalten. 
Der Client übernimmt die Datenverarbeitung und entscheidet, ob der Zustand so ist, das ein Befehl zum Anschalten der Warnleuchte erforderlich ist.
Dazu misst der Client in einem Intervall von [X] Sekunden die Distanz und wenn sie größer als [X] cm ist, wird der entsprechende Befehl geschickt.

Ein Befehl wird nur geschickt, wenn sich der aktuelle Zustand ändert. Erkennt der Client, dass die Tür offen ist, wird der Befehl nicht erneut gesendet das Warn Signal anzuschalten.

