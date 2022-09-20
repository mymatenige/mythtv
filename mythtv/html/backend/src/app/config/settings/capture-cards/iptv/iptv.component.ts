import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Observable } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-iptv',
  templateUrl: './iptv.component.html',
  styleUrls: ['./iptv.component.css']
})
export class IptvComponent implements OnInit, AfterViewInit {
  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;

  @ViewChild("iptvform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  work = {
    successCount: 0,
    errorCount: 0,
  };

  constructor(private captureCardService: CaptureCardService, private setupService: SetupService) { }

  ngOnInit(): void {
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
    if (!this.card.CardId) {
      let obs = new Observable(x => {
        setTimeout(() => {
          x.next(1);
          x.complete();
        }, 100)
      })
      obs.subscribe(x => {
        // Mark new card as dirty so it can be saved with default input value
        this.currentForm.form.markAsDirty()
      });
    }
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  // good response to add: {"int": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.card.CardId && x.bool)
        this.work.successCount++;
      else if (!this.card.CardId && x.int) {
        this.work.successCount++;
        this.card.CardId = x.int;
      }
      else {
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    console.log("save form clicked");
    this.work.successCount = 0;
    this.work.errorCount = 0;
    if (this.card.CardId) {
      // Update device and child devices
      this.cardList.CaptureCardList.CaptureCards.forEach(card => {
        if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
          this.captureCardService.UpdateCaptureCard(card.CardId, 'videodevice', this.card.VideoDevice)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'channel_timeout', String(this.card.ChannelTimeout))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }

}
