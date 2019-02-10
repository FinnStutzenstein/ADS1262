import { Component } from '@angular/core';
import { StateService } from 'src/app/services/state.service';
import { Started } from 'src/app/models/state';
import { CommandService } from 'src/app/services/command.service';
import { BaseComponent } from '../base-component';

@Component({
    selector: 'app-control',
    templateUrl: './control.component.html',
    styleUrls: ['./control.component.scss']
})
export class ControlComponent extends BaseComponent {
    public started: Started = null;
    public Started = Started;

    public constructor(private stateService: StateService, private commandService: CommandService) {
        super();

        this.stateService.getStateObservable().subscribe(state => {
            if (state) {
                this.started = state.started;
            } else {
                this.started = null;
            }
        });
    }

    public start() {
        this.commandService.sendStartCommand().catch(this.raiseErrorWithText('Could not start the ADC'));
    }

    public stop() {
        this.commandService.sendStopCommand().catch(this.raiseErrorWithText('Could not stop the ADC'));
    }
}
