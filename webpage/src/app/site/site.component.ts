import { Component } from '@angular/core';

import { ADCPRepresentationService } from '../services/adcp-representation.service';

@Component({
    selector: 'app-site',
    templateUrl: './site.component.html',
    styleUrls: ['./site.component.scss']
})
export class SiteComponent {
    public constructor(private adcpRepService: ADCPRepresentationService) {}

    public sendOk(): void {
        this.adcpRepService.respOk();
    }

    public sendError(): void {
        this.adcpRepService.respError();
    }

    public sendStatus(): void {
        this.adcpRepService.respStatus();
    }
}
