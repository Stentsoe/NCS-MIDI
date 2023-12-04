# Sequence Diagram Iso

```mermaid
sequenceDiagram
    box rgb(33,66,99) Application
    participant app as Sending thread
    end 

    box rgb(99,66,33) midi_broadcast_iso.c
    participant queue1 as tx_queue
    participant tx as tx_packet
    participant isr as radio_notif
    end

    box rgb(99,33,66) iso.c
    participant chan_send as add header
    end

    box rgb(33,99,66) conn.c
    participant queue2 as tx_queue
    participant process as process
    end

    box rgb(33,66,99) hci
    participant hci_handle as hci_iso_handle
    end

    box rgb(99,66,33) isoal.c
    participant isoal as isoal
    end

    box rgb(99,33,66) ull_iso.c
    participant queue3 as ull queue
    end
    
    box rgb(66,99,66) lll_adv_iso.c
    participant radio as radio()
    end

    radio->>+isr:radio_on
    app->>+queue1: msg 1
    queue1->>-tx: msg 1
    app->>+queue1: msg 2
    queue1->>-tx: msg 2
    isr->>-tx:time passed
    tx->>chan_send: packet
    chan_send->>+queue2: iso packet
    queue2->>-process: iso_packet
    process->>hci_handle: iso_packet
    hci_handle->>isoal: iso_packet
    isoal->>+queue3: iso_packet