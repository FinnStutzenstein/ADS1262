import { Injectable } from '@angular/core';

import { ADCPService, ADCP, ADCPStatuscodes } from './adcp.service';
import { State } from '../models/state';
import { StateService } from './state.service';
import { PacketType } from './adcp-representation.service';

@Injectable({
    providedIn: 'root'
})
export class CommandService {
    public constructor(private stateService: StateService, private adcpService: ADCPService) {}

    public async setConnectionType(types: PacketType[]): Promise<void> {
        let allTypes = 0;
        types.forEach(type => {
            allTypes = allTypes | type;
        });
        this.adcpService.sendPacket(ADCP.connection.setType, new Uint8Array([allTypes]).buffer as ArrayBuffer);
        return await this.adcpService.getStatusCodeResponse();
    }

    public async sendStartCommand(): Promise<void> {
        this.adcpService.sendPacket(ADCP.measurement.start);
        return await this.adcpService.getStatusCodeResponse();
    }

    public async sendStopCommand(): Promise<void> {
        this.adcpService.sendPacket(ADCP.measurement.stop);
        return await this.adcpService.getStatusCodeResponse();
    }

    public async queryState(): Promise<State> {
        this.adcpService.sendPacket(ADCP.adc.updateState);
        const resp = await this.adcpService.getResponse();
        const code = this.adcpService.processStatuscode(resp);

        if (code !== 0) {
            throw new Error(ADCPStatuscodes[code]);
        }

        const stateBytes = resp.slice(1, resp.byteLength);
        const state = this.stateService.constructState(stateBytes);
        this.stateService.updateState(state);
        return state;
    }
}
