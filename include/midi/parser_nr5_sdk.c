case NRF_USBD_EP_OK:
            {   
                uint8_t *buf;
                uint8_t *buf_temp;
                uint8_t len;
                uint8_t cin;
                uint8_t cable;
                app_usbd_midi_msg_t msg;

                nrf_drv_usbd_handler_desc_t handler_desc = {
                    .handler.consumer = midi_consumer,
                    .p_context        = p_midi_ctx
                };

                buf_temp = p_midi_ctx->rx_transfer[p_midi_ctx->rx_buf].data;
                len = p_midi_ctx->rx_transfer[p_midi_ctx->rx_buf].len;
                app_usbd_ep_handled_transfer(NRF_DRV_USBD_EPOUT1, &handler_desc);

                for (size_t i = 0; i < len; i += 4)
                {
                    buf = buf_temp + i;
                    cin = (*buf) & 0xF;
                    cable = (*buf) >> 4;

                    switch (cin)
                    {
                    case 0x4:
                    {
                        msg.p_data = p_midi_ctx->sysex[cable].p_data,
                        msg.len = p_midi_ctx->sysex[cable].pos;
                        
                        if (((p_midi_ctx->sysex[cable].left < 3) && msg.p_data != NULL) || 
                            (*(buf + 1) == 0xF0 && msg.p_data == NULL)) 
                        {
                            user_rx_handler(p_inst,
                                    APP_USBD_MIDI_SYSEX_BUF_REQ,
                                    cable, 
                                    &msg);
  
                            p_midi_ctx->sysex[cable].pos = 0;
                            p_midi_ctx->sysex[cable].left = msg.len;
                            p_midi_ctx->sysex[cable].p_data = msg.p_data;
                        }

                        if (p_midi_ctx->sysex[cable].p_data != NULL)
                        {
                            memcpy(p_midi_ctx->sysex[cable].p_data + p_midi_ctx->sysex[cable].pos, buf + 1, 3); 
                            p_midi_ctx->sysex[cable].pos += 3;
                            p_midi_ctx->sysex[cable].left -= 3;
                        }
                        break;
                    }
                    case 0x5:
                        if (*(buf + 1) == 0xF6)
                        {
                            msg.p_data = buf + 1;
                            msg.len = 1;
                            user_rx_handler(p_inst, APP_USBD_MIDI_RX_DONE, cable, &msg);
                            break;
                        }
                    case 0x6:
                    case 0x7:
                        if (p_midi_ctx->sysex[cable].left < cin-4 && p_midi_ctx->sysex[cable].p_data != NULL) 
                        {
                            msg.p_data = p_midi_ctx->sysex[cable].p_data,
                            msg.len = p_midi_ctx->sysex[cable].pos;

                            user_rx_handler(p_inst, APP_USBD_MIDI_SYSEX_BUF_REQ, cable, &msg);

                            p_midi_ctx->sysex[cable].pos = 0;
                            p_midi_ctx->sysex[cable].left = msg.len;
                            p_midi_ctx->sysex[cable].p_data = msg.p_data;
                        }

                        if (p_midi_ctx->sysex[cable].p_data != NULL)
                        {
                            memcpy(p_midi_ctx->sysex[cable].p_data + p_midi_ctx->sysex[cable].pos, buf + 1, cin-4);
                            p_midi_ctx->sysex[cable].pos += cin-4;

                            msg.p_data = p_midi_ctx->sysex[cable].p_data,
                            msg.len = p_midi_ctx->sysex[cable].pos;

                            user_rx_handler(p_inst, APP_USBD_MIDI_SYSEX_RX_DONE, cable, &msg);

                            p_midi_ctx->sysex[cable].pos = 0;
                            p_midi_ctx->sysex[cable].left = 0;
                            p_midi_ctx->sysex[cable].p_data = NULL;
                        }
                        break;
                    case 0xF:
                        msg.p_data = buf + 1;
                        msg.len = 1;
                        user_rx_handler(p_inst, APP_USBD_MIDI_RX_DONE, cable, &msg);
                        break;
                    case 0x2:
                    case 0xC:
                    case 0xD:
                        msg.p_data = buf + 1;
                        msg.len = 2;
                        user_rx_handler(p_inst, APP_USBD_MIDI_RX_DONE, cable, &msg);
                        break;
                    default:
                        msg.p_data = buf + 1;
                        msg.len = 3;
                        user_rx_handler(p_inst, APP_USBD_MIDI_RX_DONE, cable, &msg);
                        break;
                    }
                }
                return NRF_SUCCESS;
                
            }