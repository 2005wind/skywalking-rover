// Licensed to Apache Software Foundation (ASF) under one or more contributor
// license agreements. See the NOTICE file distributed with
// this work for additional information regarding copyright
// ownership. Apache Software Foundation (ASF) licenses this file to you under
// the Apache License, Version 2.0 (the "License"); you may
// not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package process

import "github.com/apache/skywalking-rover/pkg/process/api"

type Operator interface {
	// FindProcessById the processID is received from the backend, if not found then return nil
	FindProcessByID(processID string) api.ProcessInterface
	// FindProcessByPID get all processes with difference entity through process PID
	FindProcessByPID(pid int32) []api.ProcessInterface
	// FindAllRegisteredProcesses find all registered processes
	FindAllRegisteredProcesses() []api.ProcessInterface
	// AddListener add new process listener
	AddListener(listener api.ProcessListener)
	// DeleteListener delete the process listener
	DeleteListener(listener api.ProcessListener)
	// ShouldMonitor check the process should be monitored
	ShouldMonitor(pid int32) bool
}

type K8sOperator interface {
	// NodeName get the node name
	NodeName() string
}
