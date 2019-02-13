import { Injectable } from '@angular/core';

import { ADCPService, ADCPStatuscodes, ADCPPrefixCommand } from './adcp.service';
import { State } from '../models/state';
import { StateService } from './state.service';
import { PacketType } from './adcp-representation.service';

/**
 * All currently implemented commands.
 */
export const ADCP: {
    [prefix: string]: {
        [command: string]: ADCPPrefixCommand;
    };
} = {
    connection: {
        setType: [0x10, 0x00]
    },
    measurement: {
        start: [0x12, 0x01],
        stop: [0x12, 0x02]
    },
    adc: {
        updateState: [0x13, 0x07]
    }
};

/**
 * Takes care about commenda from ADCP.
 */
@Injectable({
    providedIn: 'root'
})
export class CommandService {
    public constructor(private stateService: StateService, private adcpService: ADCPService) {}

    /**
     * Sets the connection type
     *
     * @param types A list of types of messages to recieve
     */
    public async setConnectionType(types: PacketType[]): Promise<void> {
        let allTypes = 0;
        types.forEach(type => {
            allTypes = allTypes | type;
        });
        this.adcpService.sendPacket(ADCP.connection.setType, new Uint8Array([allTypes]).buffer as ArrayBuffer);
        return await this.adcpService.getStatusCodeResponse();
    }

    /**
     * Sends the start command.
     */
    public async sendStartCommand(): Promise<void> {
        this.adcpService.sendPacket(ADCP.measurement.start);
        return await this.adcpService.getStatusCodeResponse();
    }

    /**
     * Sends the stop command.
     */
    public async sendStopCommand(): Promise<void> {
        this.adcpService.sendPacket(ADCP.measurement.stop);
        return await this.adcpService.getStatusCodeResponse();
    }

    /**
     * Queries the current state from the server.
     */
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
