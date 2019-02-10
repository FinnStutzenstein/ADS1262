import { Injectable } from '@angular/core';

import { Subject, Observable } from 'rxjs';
import { ADCPRepresentationService, PacketType } from './adcp-representation.service';
import { StructService } from './struct.service';
import { appendBuffers } from './arraybuffer-helper';

export interface FFTUpdate {
    psd: number[];
    N: number;
    wss: number;
    fRes: number;
}

@Injectable({
    providedIn: 'root'
})
export class FFTService {
    private fftSubject = new Subject<FFTUpdate>();

    private id: number;
    private lastFrameNumber: number;
    private dataBuffer: ArrayBuffer;

    public constructor(private adcpRepService: ADCPRepresentationService, private structService: StructService) {
        this.reset();
        this.adcpRepService.getPacketObservable(PacketType.FFT).subscribe(buffer => this.rawInput(buffer));
    }

    public getFFTObservalbe(): Observable<FFTUpdate> {
        return this.fftSubject.asObservable();
    }

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

        console.log('got frame ' + frameNumber + '/' + frameCount);

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

        if (frameNumber + 1 === frameCount) {
            // OK. finished. Process data.
            this.processPacketData(this.dataBuffer, N, wss, resolution);
            this.reset();
        } else {
            this.dataBuffer = appendBuffers(this.dataBuffer, metainfos[7] as ArrayBuffer);
        }
    }

    private reset() {
        this.lastFrameNumber = -1;
        this.dataBuffer = new ArrayBuffer(0);
    }

    private processPacketData(buffer: ArrayBuffer, N: number, wss: number, fRes: number): void {
        // TODO
        this.input([], N, wss, fRes);
    }

    private input(rawFFTData: number[], N: number, wss: number, fRes: number) {
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

        psd = psd.map(val => val + Math.floor(Math.random() * 5));

        this.fftSubject.next({
            psd: psd,
            N: N,
            wss: wss,
            fRes: fRes
        });
    }
}
