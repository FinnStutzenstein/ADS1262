import { Injectable } from '@angular/core';

import { Subject, Observable } from 'rxjs';

import { ADCPRepresentationService, PacketType } from './adcp-representation.service';
import { StructService } from './struct.service';
import { appendBuffers } from './arraybuffer-helper';

/**
 * Encapsulates all information about an PSD update.
 */
export interface PSDUpdate {
    psd: number[];
    N: number;
    wss: number;
    fRes: number;

    /**
     * The measurement id.
     */
    id: number;
}

/**
 * Takes care of recieving FFT messages. Converts them to the PSD. With `getPSDObservable` you can
 * get full updates of new PSD data.
 * Currently works only for one measurement. The first FFT message recieved determinates the id.
 */
@Injectable({
    providedIn: 'root'
})
export class PSDService {
    /**
     * The subject with the PSD data
     */
    private psdSubject = new Subject<PSDUpdate>();

    /**
     * The one measurement to listen to.
     */
    private id: number;

    /**
     * Saves the last frame number recieved.
     */
    private lastFrameNumber: number;

    /**
     * Accumulates the data, if the FFT message is fragmented.
     */
    private dataBuffer: ArrayBuffer;

    public constructor(private adcpRepService: ADCPRepresentationService, private structService: StructService) {
        this.reset();
        this.adcpRepService.getPacketObservable(PacketType.FFT).subscribe(buffer => this.rawInput(buffer));
    }

    public getPSDObservalbe(): Observable<PSDUpdate> {
        return this.psdSubject.asObservable();
    }

    /**
     * Raw input of an fft message. Unpacks the metainfo and accumulates the data.
     * If a full message is collected, passes the data to `processPacketData`.
     *
     * @param buffer The fft message
     */
    private rawInput(buffer: ArrayBuffer): void {
        const metadatasize = 21;
        if (buffer.byteLength < metadatasize) {
            return;
        }

        const metainfos = this.structService.fromBuffer('BBBHQffA', buffer);

        const id = metainfos[0] as number;
        if (!this.id) {
            this.id = id;
        } else if (this.id !== id) {
            return;
        }

        const frameCount = metainfos[1] as number;
        const frameNumber = metainfos[2] as number;
        const N = metainfos[3] as number;
        // timestamp not needed..
        const resolution = metainfos[5] as number;
        const wss = metainfos[6] as number;

        //console.log('got frame ' + (frameNumber + 1) + '/' + frameCount);

        if (this.lastFrameNumber + 1 !== frameNumber) {
            console.log('Drop FFT Frame');
            this.reset();
            if (frameNumber !== 0) {
                return;
                // if we somehow got into a transmission of frames (e.g. starting
                // by the second one), skip this.
            }
        }
        this.lastFrameNumber = frameNumber;

        this.dataBuffer = appendBuffers(this.dataBuffer, metainfos[7] as ArrayBuffer);
        if (frameNumber + 1 === frameCount) {
            // OK. finished. Process data.
            this.processPacketData(this.dataBuffer, N, wss, resolution);
            this.reset();
        }
    }

    /**
     * Resets the aquiring of the current package.
     */
    private reset() {
        this.lastFrameNumber = -1;
        this.dataBuffer = new ArrayBuffer(0);
    }

    /**
     * Processes the payload of a full fft message.
     *
     * @param buffer Payload
     * @param N
     * @param wss
     * @param fRes
     */
    private processPacketData(buffer: ArrayBuffer, N: number, wss: number, fRes: number): void {
        if (buffer.byteLength % 8 !== 0) {
            throw new Error("ByteLength of samples is not a multiple of 8!");
        }
        if (buffer.byteLength !== (N * 4)) {
            throw new Error("Must recieve " + N + " floats, got " + buffer.byteLength + " bytes");
        }

        const rawFFTData: number[] = [];
        const view = new DataView(buffer);
        for (let i = 0; i < buffer.byteLength; i += 4) {
            rawFFTData.push(view.getFloat32(i, true));
        }

        this.calcPSD(rawFFTData, N, wss, fRes);
    }

    /**
     * Calculates the PSD of the recieved raw FFT data. Publishes the result via the psdSubject.
     *
     * @param rawFFTData The raw data of `N` numbers representing N/2 complex numbers.
     * @param N
     * @param wss
     * @param fRes
     */
    private calcPSD(rawFFTData: number[], N: number, wss: number, fRes: number) {
        const NHalf = N / 2;
        const fft = [];

        fft.push(rawFFTData[0]);
        for (let i = 2; i < 1024; i += 2) {
            const re = rawFFTData[i];
            const im = rawFFTData[i + 1];
            fft.push(Math.sqrt(re * re + im * im));
        }
        fft.push(rawFFTData[1]);

        // PSD
        let psd = fft.map(val => (2.0 * val * val) / (fRes * N * wss));
        psd[0] = psd[0] / 2.0;
        psd[NHalf] = psd[NHalf] / 2.0;
        psd = psd.map(val => 10 * Math.log10(val));

        this.psdSubject.next({
            psd: psd,
            N: N,
            wss: wss,
            fRes: fRes,
            id: this.id
        });
    }
}
