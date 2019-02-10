import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';

import { ChartModule } from 'angular-highcharts';

import { SiteComponent } from './site.component';
import { SiteRoutingModule } from './site-routing.module';
import { DebugComponent } from './components/debug/debug.component';
import { ConnectComponent } from './components/connect/connect.component';
import { StateComponent } from './components/state/state.component';
import { ControlComponent } from './components/control/control.component';
import { FFTComponent } from './components/fft/fft.component';

@NgModule({
    declarations: [SiteComponent, DebugComponent, ConnectComponent, StateComponent, ControlComponent, FFTComponent],
    imports: [CommonModule, SiteRoutingModule, ChartModule],
    providers: []
})
export class SiteModule {}
