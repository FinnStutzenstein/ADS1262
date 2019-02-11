import { Component } from '@angular/core';
import { StateService } from 'src/app/services/state.service';
import { State } from 'src/app/models/state';

@Component({
    selector: 'app-state',
    templateUrl: './state.component.html',
    styleUrls: ['./state.component.scss']
})
export class StateComponent {
    public state: State;

    public constructor(private stateService: StateService) {
        this.state = this.stateService.getCurrentState();
        this.stateService.getStateObservable().subscribe(state => (this.state = state));
    }
}
