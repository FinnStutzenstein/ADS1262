import { Component, ViewChild, OnInit } from '@angular/core';
import { WebsocketService } from 'src/app/services/websocket.service';
import { PacketType } from 'src/app/services/adcp-representation.service';
import { ADCPService } from 'src/app/services/adcp.service';
import { CommandService } from 'src/app/services/command.service';
import { BaseComponent } from '../base-component';

@Component({
    selector: 'app-connect',
    templateUrl: './connect.component.html',
    styleUrls: ['./connect.component.scss']
})
export class ConnectComponent extends BaseComponent implements OnInit {
    public connected: boolean;

    @ViewChild('ipaddress')
    public ipaddress;

    public connectedIpAddress = '192.168.1.21';

    public constructor(
        private ws: WebsocketService,
        private adcpService: ADCPService,
        private commandService: CommandService
    ) {
        super();

        this.ws.getConnectionEventObservable().subscribe(() => {
            this.commandService.setConnectionType([PacketType.Debug, PacketType.Status, PacketType.FFT]).then(
                () => {
                    this.commandService.queryState().catch(this.raiseErrorWithText('Could not query initial state'));
                },
                error => {
                    this.raiseErrorWithText('Could not connect')(error);
                    this.ws.disconnect();
                }
            );

            this.connected = true;
        });
        this.ws.getCloseEventObservable().subscribe(() => {
            this.connected = false;
            this.updateIpAddressField();
        });
    }

    public ngOnInit(): void {
        this.updateIpAddressField();
    }

    public updateIpAddressField(): void {
        // Wait for the input to be rendered, because it must check the *ngIf
        setTimeout(() => {
            if (this.ipaddress) {
                this.ipaddress.nativeElement.value = this.connectedIpAddress;
            }
        }, 1);
    }

    public async connect(): Promise<void> {
        this.connectedIpAddress = this.ipaddress.nativeElement.value;
        this.ws.connect(this.connectedIpAddress);
    }

    public disconnect(): void {
        this.ws.disconnect();
    }
}
