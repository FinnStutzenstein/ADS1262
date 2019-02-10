import { Component, OnInit } from '@angular/core';

import { Chart } from 'angular-highcharts';

import { FFTUpdate, FFTService } from 'src/app/services/fft.service';

@Component({
    selector: 'app-fft',
    templateUrl: './fft.component.html',
    styleUrls: ['./fft.component.scss']
})
export class FFTComponent implements OnInit {
    public chart: Chart;

    public fRes: number;
    public N: number;
    public wss: number;
    public averagingCount = 0;
    public psdAveraging: number[];

    public constructor(private fftService: FFTService) {
        this.chart = new Chart({
            chart: {
                type: 'line'
            },
            title: {
                text: 'PSD'
            },
            credits: {
                enabled: false
            },
            series: [
                {
                    type: 'line',
                    name: 'DFT',
                    data: []
                }
            ],
            yAxis: [
                {
                    title: {
                        text: 'Test'
                    }
                }
            ],
            xAxis: [
                {
                    title: {
                        text: 'X Achse'
                    },
                    type: 'logarithmic'
                }
            ]
        });
    }

    public ngOnInit(): void {
        this.fftService.getFFTObservalbe().subscribe(update => this.update(update));
    }

    public update(update: FFTUpdate): void {
        if (this.wss !== update.wss || this.N !== update.N) {
            this.reset();
        }

        this.averagingCount++;

        if (!this.psdAveraging) {
            this.psdAveraging = update.psd;
        } else {
            this.psdAveraging = this.psdAveraging.map((value, index) => {
                return value + update.psd[index];
            });
        }

        const points: [number, number][] = this.psdAveraging.map((val, index) => {
            val /= this.averagingCount;
            if (index === 0) {
                return [1, val] as [number, number];
            } else {
                return [index * update.fRes, val] as [number, number];
            }
        });

        this.chart.ref$.subscribe(chart => {
            chart.series[0].setData(points, true);
        });

        this.fRes = update.fRes;
        this.N = update.N;
        this.wss = update.wss;
    }

    public reset(): void {
        this.psdAveraging = null;
        this.averagingCount = 0;
    }
}
