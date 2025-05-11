import { Component, ElementRef, OnInit, QueryList, ViewChild, ViewChildren } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { MenuItem, MessageService } from 'primeng/api';
import { Table, TableLazyLoadEvent } from 'primeng/table';
import { ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { GetUpcomingRequest } from 'src/app/services/interfaces/dvr.interface';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { RecRule } from 'src/app/services/interfaces/recording.interface';
import { UtilityService } from 'src/app/services/utility.service';
import { Menu } from 'primeng/menu';

interface RuleListEntry {
  Id: number;
  Title: string;
}


@Component({
  selector: 'app-upcoming',
  templateUrl: './upcoming.component.html',
  styleUrls: ['./upcoming.component.css'],
  providers: [MessageService]
})
export class UpcomingComponent implements OnInit, SchedulerSummary {

  @ViewChild('table') table!: Table;
  @ViewChildren('row') rows!: QueryList<ElementRef>;
  @ViewChild("menu") menu!: Menu;

  programs: ScheduleOrProgram[] = [];
  selection: ScheduleOrProgram[] = [];
  actionList: ScheduleOrProgram[] = [];
  recRules: RuleListEntry[] = [];
  allRecRules: RuleListEntry[] = [];
  activeRecRules: RuleListEntry[] = [];
  defaultRecRule: RuleListEntry = { Id: 0, Title: 'settings.chanedit.all' };
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  refreshing = false;
  loaded = false;
  inter: ScheduleLink = { summaryComponent: this };
  lazyLoadEvent!: TableLazyLoadEvent;

  displayStop = false;
  errorCount = 0;
  program?: ScheduleOrProgram;
  totalRecords = 0;
  showTable = false;
  virtualScrollItemSize = 0;
  selectedRule: RuleListEntry | null = null;
  selectedStatus = '';

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    ActionsSelected: 'dashboard.upcoming.actionsselected',
    RestartError: 'dashboard.upcoming.restart_err',
    UndefSelection: 'dashboard.undefselection'
  }

  // Menu Items
  mnu_dontrec: MenuItem = { label: 'dashboard.upcoming.mnu_dontrec', command: (event) => this.editUpcoming(event, false) };
  mnu_norec: MenuItem = { label: 'dashboard.prevrecs.mnu_norec', command: (event) => this.editUpcoming(event, true) };

  menuToShow: MenuItem[] = [];

  constructor(private dvrService: DvrService, private messageService: MessageService,
    private translate: TranslateService, public dataService: DataService,
    private utility: UtilityService) {
    this.translate.get(this.defaultRecRule.Title).subscribe(data => this.defaultRecRule.Title = data);

    // translations
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }
    const mnu_entries = [this.mnu_dontrec, this.mnu_norec];
    mnu_entries.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data =>
          entry.label = data
        );
    });

    this.loadRecRules();
  }

  ngOnInit(): void {
    // Initial Load
    this.loadLazy({ first: 0, rows: 1 });
  }

  refresh() {
    this.refreshing = true;
    this.showTable = false;
    this.loadRecRules();
    this.loadLazy(this.lazyLoadEvent);
  }

  loadRecRules() {
    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.allRecRules.length = 0;
        this.allRecRules.push(this.defaultRecRule);
        this.activeRecRules.length = 0;
        this.activeRecRules.push(this.defaultRecRule);
        data.RecRuleList.RecRules.forEach((entry) => {
          if (entry.Type != 'Recording Template') {
            let recRule = {
              Id: entry.Id,
              Title: entry.Title.substring(0, 30) + ' [' + this.utility.recTypeTrans[entry.Type] + ']'
            };
            this.allRecRules.push(recRule);
            if (entry.NextRecording) {
              this.activeRecRules.push(recRule);
            };
          }
        });
        this.recRules.length = 0;
        if (this.selectedStatus == 'All')
          this.recRules.push(...this.allRecRules)
        else
          this.recRules.push(...this.activeRecRules)
      },
    });
  }

  loadLazy(event: TableLazyLoadEvent) {
    this.lazyLoadEvent = event;

    let request: GetUpcomingRequest = {
      StartIndex: 0,
      Count: 1,
      ShowAll: false
    };
    if (event.first != undefined) {
      request.StartIndex = event.first;
      if (event.last)
        request.Count = event.last - event.first;
      else if (event.rows)
        request.Count = event.rows;
    }
    let sortField = '';
    if (Array.isArray(event.sortField))
      sortField = event.sortField[0];
    else if (event.sortField)
      sortField = event.sortField;
    if (!sortField)
      sortField = 'StartTime';
    if (sortField == 'Channel.ChanNum')
      request.Sort = 'ChanNum';
    else
      request.Sort = sortField;
    let sortOrder = '';
    if (event.sortOrder && event.sortOrder < 0)
      sortOrder = ' desc';
    request.Sort = request.Sort + sortOrder;

    if (this.selectedStatus == 'All')
      request.ShowAll = true;
    else if (this.selectedStatus && this.selectedStatus != 'Default')
      request.RecStatus = this.selectedStatus;
    if (this.selectedRule != null && this.selectedRule.Id != 0)
      request.RecordId = this.selectedRule.Id;
    this.recRules.length = 0;
    if (request.ShowAll)
      this.recRules.push(...this.allRecRules)
    else
      this.recRules.push(...this.activeRecRules)
    this.dvrService.GetUpcomingList(request).subscribe(data => {
      let recordings = data.ProgramList;
      this.totalRecords = data.ProgramList.TotalAvailable;
      this.programs.length = this.totalRecords;
      // populate page of virtual programs
      // note that Count is returned as the count requested, even
      // if less items are returned because you hit the end.
      // Maybe we should use recordings.Programs.length
      this.programs.splice(recordings.StartIndex, recordings.Count,
        ...recordings.Programs);
      // notify of change
      this.programs = [...this.programs]
      this.refreshing = false;
      this.showTable = true;
      let row = this.rows.get(0);
      if (row && row.nativeElement.offsetHeight)
        this.virtualScrollItemSize = row.nativeElement.offsetHeight;
      if (this.table) {
        this.table.totalRecords = this.totalRecords;
        this.table.virtualScrollItemSize = this.virtualScrollItemSize;
      }
    });

  }

  onFilter() {
    this.reload();
  }

  reload() {
    this.showTable = false;
    this.programs.length = 0;
    this.refreshing = true;
    this.loadLazy(({ first: 0, rows: 1 }));
  }

  formatStartDate(program: ScheduleOrProgram, rowIndex: number): string {
    let priorDate = '';
    if (rowIndex > 0 && this.programs[rowIndex - 1]
      && this.programs[rowIndex - 1].Recording.StartTs)
      priorDate = this.utility.formatDate(this.programs[rowIndex - 1].Recording.StartTs, true, true);
    let thisDate = this.utility.formatDate(program.Recording.StartTs, true, true);
    if (priorDate == thisDate)
      return ' ';
    return thisDate;
  }

  formatAirDate(program: ScheduleOrProgram): string {
    if (!program.Airdate)
      return ' ';
    let date = program.Airdate + ' 00:00';
    return this.utility.formatDate(date, true);
  }

  formatStartTime(program: ScheduleOrProgram): string {
    const tWithSecs = new Date(program.Recording.StartTs).toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getDuration(program: ScheduleOrProgram): number {
    let starttm = new Date(program.Recording.StartTs).getTime();
    let endtm = new Date(program.Recording.EndTs).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  updateRecRule(program: ScheduleOrProgram) {
    if (this.inter.sched)
      this.inter.sched.open(program);
  }

  override(program: ScheduleOrProgram) {
    if (this.inter.sched) {
      if (program.Recording.RecType == 7 || program.Recording.RecType == 8
        || program.Recording.StatusName == 'NeverRecord') // If already an override
        this.inter.sched.open(program);
      else
        this.inter.sched.open(program, undefined, <RecRule>{ Type: 'Override Recording' });
    }
  }

  stopRequest(program: ScheduleOrProgram) {
    if (program.Recording.RecordedId) {
      this.program = program;
      this.displayStop = true;
    }
  }

  restartRequest(program: ScheduleOrProgram) {
    if (program.Recording.RecordId) {
      this.program = program;
      this.errorCount = 0;
      this.dvrService.ReactivateRecording({
        ChanId: program.Channel.ChanId,
        RecordId: program.Recording.RecordId
      }).subscribe({
        next: (x) => {
          if (x.bool) {
            setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          }
          else {
            this.errorCount++;
            console.log("Error: dvrService.ReactivateRecording returned false");
            this.sendMessage('error', null, '', this.msg.RestartError);
          }
        },
        error: (err) => {
          this.errorCount++;
          console.log("Error: dvrService.ReactivateRecording returned http error");
          this.sendMessage('error', null, '', this.msg.RestartError);
        }
      });
    }
    else {
      console.log("Error: there is no RecordId for the entry");
      this.sendMessage('error', null, '', this.msg.RestartError);
    }
  }

  sendMessage(severity: string, program: ScheduleOrProgram | null, action: string, text: string, extraText?: string) {
    if (extraText)
      extraText = '\n' + extraText;
    else
      extraText = '';
    let detail = action;
    if (program != null)
      detail = action + ' ' + program.Title + ' ' + program.SubTitle + extraText;
    this.messageService.add({
      severity: severity, summary: text,
      detail: detail,
      life: 5000,
      sticky: (severity == 'error')
    });
  }

  stopRecording(program: ScheduleOrProgram) {
    this.errorCount = 0;
    this.dvrService.StopRecording(program.Recording.RecordedId)
      .subscribe({
        next: (x) => {
          if (x.bool) {
            this.displayStop = false;
            setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          }
          else
            this.errorCount++;
        },
        error: (err) => {
          this.errorCount++;
        }
      });
  }

  // return true causes default browser right click menu to show
  // return false suppresses default browser right click menu
  onContextMenu(program: ScheduleOrProgram, event: any) {
    if (this.selection.length == 0)
      return true;
    if (this.selection.some((x) => !x)) {
      // This happens if some entries have not been loaded
      this.sendMessage('error', null, '', this.msg.UndefSelection);
      return false;
    }
    if (this.selection.some((x) => x.Recording.RecordedId == program.Recording.RecordedId)) {
      this.showContextMenu(null, event);
      return false;
    }
    return true;
  }

  onSelectChange() {
    this.menu.hide();
  }

  showContextMenu(program: ScheduleOrProgram | null, event: any) {
    this.actionList.length = 0;
    if (program && program.Title)
      this.actionList.push(program);
    else
      this.actionList.push(...this.selection);
    if (this.actionList.length == 0)
      return;
    if (this.actionList.some((x) => !x)) {
      this.sendMessage('error', null, '', this.msg.UndefSelection);
      return;
    }
    this.menuToShow.length = 0;
    let subMenu: MenuItem[] = [];
    if (this.actionList.some((x) => x.Recording.StatusName == 'WillRecord')) {
      subMenu.push(this.mnu_dontrec);
      subMenu.push(this.mnu_norec);

      if (this.actionList.length == 1) {
        this.menuToShow.push({ label: this.actionList[0].Title + ' - ' + this.actionList[0].SubTitle, items: subMenu });
      }
      else
        this.menuToShow.push({ label: this.msg.ActionsSelected.replace(/{{ *num *}}/, this.actionList.length.toString()), items: subMenu });
    }

    // Notify Angular that menu has changed
    this.menuToShow = [...this.menuToShow];
    this.menu.toggle(event);
  }

  editUpcoming(event: any, never: boolean) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.AddDontRecordSchedule({
        ChanId: program.Channel.ChanId,
        StartTime: program.StartTime,
        NeverRecord: never
      }).subscribe({
        next: (x) => {
          if (x.bool)
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          else
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          this.editUpcoming(event, never);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.editUpcoming(event, never);
        }
      });
    }
    else {
      setTimeout( () => this.refresh(), 1000 );
    }
  }

  networkError(program: ScheduleOrProgram, err: any) {
    console.log("network error", err);
    this.sendMessage('error', program, '', this.msg.NetFail);
  }

}
