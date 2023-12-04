# Sequence Diagram Iso

```mermaid
sequenceDiagram
    box rgb(33,66,99) Application
    participant app as Sending thread
    end 

    box rgb(99,66,33) midi_broadcast_iso.c
    participant send as send_to_iso_broadcaster_port()
    participant queue as tx_queue
    participant encode as iso_encode_packet_work_handler()
    participant tx as iso_tx_work_handler()
    participant isr as radio_notif()
    end

    box rgb(99,33,66) iso.c
    participant chan_send as bt_iso_chan_send()
    end



    box rgb(66,99,66) iso.c
    participant radio as radio()
    end
    radio->>+isr:radio_on
    app->>send: msg 1
    send->>+queue: msg 1
    queue->>-encode: msg 1
    app->>send: msg 2
    send->>+queue: msg 2
    queue->>-encode: msg 2
    encode->>tx: packet
    isr->>-tx:time passed
    tx->>chan_send: packet