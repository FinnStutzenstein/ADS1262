import { Component } from '@angular/core';
import { WebsocketService } from '../services/websocket.service';
import { ADCPService } from '../services/adcp.service';
import { DebugService } from '../services/debug.service';

@Component({
    selector: 'app-site',
    templateUrl: './site.component.html'
})
export class SiteComponent {
    title = 'ads1262';

    public constructor(private ws: WebsocketService, private adcp: ADCPService, private debugService: DebugService) {
        console.log(ws);

        this.debugService.getDebugMessageObservable().subscribe(msg => {
            console.log('DEBUG', msg);
        });
    }

    public async connect(): Promise<void> {
        this.ws.connect('192.168.1.20');
    }
}
