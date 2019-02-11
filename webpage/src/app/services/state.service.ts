import { Injectable } from '@angular/core';

import { Observable, BehaviorSubject } from 'rxjs';

import { ADCPRepresentationService, PacketType } from './adcp-representation.service';
import { StructService } from './struct.service';
import { State, MeasurementState } from '../models/state';
import { WebsocketService } from './websocket.service';

/**
 * Takes care about recieving state updates. Parses them and publish them via
 * `getStateObservable`. Query the current state via `getCurrentState`.
 */
@Injectable({
    providedIn: 'root'
})
export class StateService {
    private readonly statusSubject: BehaviorSubject<State> = new BehaviorSubject<State>(null);

    public constructor(
        private ADCPRepService: ADCPRepresentationService,
        private structService: StructService,
        private websocketService: WebsocketService
    ) {
        this.ADCPRepService.getPacketObservable(PacketType.Status).subscribe((msg: ArrayBuffer) => {
            try {
                console.log("got statusupdate");
                const state = this.constructState(msg);
                this.updateState(state);
            } catch (e) {
                console.log("could not parse state.");
            }
        });

        /*
        //                   |       |       |       |       |       |       |       |
        const bytesString = '1C04FF00e6b2000e0000000005a80000031c00410101130100000000f000';

        const view = new Uint8Array(bytesString.length / 2);
        for (let i = 0; i < bytesString.length; i += 2) {
            view[i / 2] = parseInt(bytesString.substring(i, i + 2), 16);
        }

        const state = this.constructState(view.buffer as ArrayBuffer);
        //this.updateState(state);*/

        this.websocketService.getCloseEventObservable().subscribe(() => {
            this.updateState(null);
        });
    }

    /**
     * Sets the given state as the current state.
     *
     * @param state The new state.
     */
    public updateState(state: State | null): void {
        this.statusSubject.next(state);
    }

    /**
     * Build the state from the given bytes.
     *
     * @param bytes The bytes.
     */
    public constructState(bytes: ArrayBuffer): State {
        const adcStateSize = 21;
        if (bytes.byteLength < adcStateSize) {
            throw new Error("The server didn't send enough data");
        }

        const result = this.structService.fromBuffer('BBBQBiIB', bytes.slice(0, adcStateSize));
        const state = new State();

        // Parse the state.
        const status = result[0] as number;
        state.started = status & 0x03;
        state.internalReference = !!(status & 0x04);
        state.slowConnection = !!(status & 0x08);
        state.ADCReset = !!(status & 0x10);

        const srFilter = result[1] as number;
        state.samplerate = srFilter & 0x0f;
        state.filter = (srFilter & 0xf0) >> 4;

        state.pga = result[2] as number;
        state.vRef = (result[3] as bigInt.BigInteger).toJSNumber();
        const vRefInputs = result[4] as number;
        state.vRefPos = vRefInputs & 0x0f;
        state.vRRefNeg = (vRefInputs & 0xf0) >> 4;

        state.calibrationOffset = result[5] as number;
        state.calibrationScale = result[6] as number;
        const measurementCount = result[7] as number;

        // check for length of all measurements
        const measurementStateSize = 9;
        const expectedLength = adcStateSize + measurementCount * measurementStateSize;
        if (bytes.byteLength < expectedLength) {
            throw new Error("The server didn't send enough data");
        }
        // Create measurement states.
        for (let i = adcStateSize; i < expectedLength; i += measurementStateSize) {
            const m = this.constructMeasurementState(bytes.slice(i, i + measurementStateSize));
            state.measurements[m.id] = m;
        }

        return state;
    }

    /**
     * Builds a measurementState from the given bytes.
     *
     * @param bytes The measurement state bytes.
     */
    private constructMeasurementState(bytes: ArrayBuffer): MeasurementState {
        const result = this.structService.fromBuffer('BBBHBHB', bytes);
        const measurementState = new MeasurementState();

        measurementState.id = result[0] as number;
        const inputs = result[1] as number;
        measurementState.pos = inputs & 0x0f;
        measurementState.neg = (inputs & 0xf0) >> 4;
        measurementState.enabled = !!result[2];
        measurementState.averaging = result[3] as number;

        measurementState.fftEnabled = !!(result[4] as number);
        measurementState.fftLength = result[5] as number;
        measurementState.fftWindow = result[6] as number;

        return measurementState;
    }

    /**
     * @return the current state or null, of there is no state.
     */
    public getCurrentState(): State | null {
        return this.statusSubject.getValue();
    }

    /**
     * @returns the state observable.
     */
    public getStateObservable(): Observable<State | null> {
        return this.statusSubject.asObservable();
    }
}
