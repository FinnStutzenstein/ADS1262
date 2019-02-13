export enum Started {
    Idle = 0,
    Running = 1,
    OneSot = 2,
    Calibrating = 3
}

export class MeasurementState {
    public id: number;
    public pos: number;
    public neg: number;
    public enabled: boolean;
    public averaging: number;

    public fftEnabled: boolean;
    public fftLength: number;
    public fftWindow: number;
    public get verboseFftWindow(): string {
        if (this.fftWindow >= 0 && this.fftWindow <= 2) {
            return ['Hann', 'Berlett', 'Welch'][this.fftWindow];
        } else if (this.fftWindow === 0xff) {
            return 'Keine Fensterfunktion';
        } else {
            return 'Unbekannte Fensterfunction';
        }
    }

    public constructor() {}
}

export class State {
    private static SamplerateLookup = [
        '2,5',
        '5',
        '10',
        '16,6',
        '20',
        '50',
        '60',
        '100',
        '400',
        '1200',
        '2400',
        '4800',
        '7200',
        '14400',
        '19200',
        '38400'
    ];

    private static FilterLookup = ['sinc1', 'sinc2', 'sinc3', 'sinc4', 'FIR'];
    private static PGALookup = ['1', '2', '4', '8', '16', '32'];
    private static RefPosPinLookup = ['0', '2', '4', 'AVDD'];
    private static RefNegPinLookup = ['1', '3', '5', 'AVSS'];
    private static StartedLookup = ['Idle', 'Running', 'Oneshot', 'Calibrating'];

    public started: Started;
    public get verboseStarted(): string {
        return State.StartedLookup[this.started];
    }
    public internalReference: boolean;
    public slowConnection: boolean;
    public ADCReset: boolean;

    public samplerate: number;
    public get verboseSamplerate(): string {
        try {
            return State.SamplerateLookup[this.samplerate];
        } catch (e) {
            return 'Unbekannte Samplerate';
        }
    }

    public filter: number;
    public get verboseFilter(): string {
        try {
            return State.FilterLookup[this.filter];
        } catch (e) {
            return 'Unbekannter Filter';
        }
    }

    public pga: number;
    public get verbosePGA(): string {
        try {
            if (this.pga === 0xff) {
                return 'Bypassed';
            } else {
                return State.PGALookup[this.pga];
            }
        } catch (e) {
            return 'Unbekannte PGA Einstellung';
        }
    }

    public vRef: number;
    public vRefPos: number;
    public get verboseVRefPos(): string {
        try {
            return State.RefPosPinLookup[this.vRefPos];
        } catch (e) {
            return 'Unbekannter Eingang';
        }
    }
    public vRRefNeg: number;
    public get verboseVRefNeg(): string {
        try {
            return State.RefNegPinLookup[this.vRRefNeg];
        } catch (e) {
            return 'Unbekannter Eingang';
        }
    }

    public calibrationOffset: number;
    public get calibrationOffsetDiff(): string {
        const diff_nv = (this.vRef * 10 * -this.calibrationOffset) / Math.pow(2, 24)
        return diff_nv.toFixed(2) + ' nV';
    }
    public calibrationScale: number;
    public get calibrationScaleDiff(): string {
        const diff = (this.calibrationScale / 0x400000) - 1.0;
        if (Math.abs(diff) > 0.01) {
            return (diff * 100).toFixed(2) + ' %';
        } else {
            return (diff * 1000000).toFixed(2) + ' ppm';
        }
    }

    public measurements: { [id: number]: MeasurementState };
    public get measurementCount(): number {
        return Object.values(this.measurements).length;
    }
    public get viewMeasurements(): MeasurementState[] {
        return Object.values(this.measurements).sort((a, b) => a.id - b.id);
    }

    public constructor() {
        this.measurements = {};
    }
}
