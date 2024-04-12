// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';

import {Icons} from '../base/semantic_icons';
import {Actions} from '../common/actions';

import {globals} from './globals';
import {Button} from '../widgets/button';

export interface CloseTrackButtonAttrs {
  trackKey: string;
}

export class CloseTrackButton
  implements m.ClassComponent<CloseTrackButtonAttrs>
{
  view({attrs}: m.CVnode<CloseTrackButtonAttrs>) {
    return m(Button, {
      onclick: () => {
        globals.dispatch(Actions.removeTracks({trackKeys: [attrs.trackKey]}));
      },
      icon: Icons.Close,
      title: 'Close',
      minimal: true,
      compact: true,
    });
  }
}
