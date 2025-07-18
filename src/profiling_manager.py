#!/usr/bin/env python3
'''
/*******************************************************************************
*   Copyright (C) 2025 Cardinal Space Mining Club                              *
*                                                                              *
*                                 ;xxxxxxx:                                    *
*                                ;$$$$$$$$$       ...::..                      *
*                                $$$$$$$$$$x   .:::::::::::..                  *
*                             x$$$$$$$$$$$$$$::::::::::::::::.                 *
*                         :$$$$$&X;      .xX:::::::::::::.::...                *
*                 .$$Xx++$$$$+  :::.     :;:   .::::::.  ....  :               *
*                :$$$$$$$$$  ;:      ;xXXXXXXXx  .::.  .::::. .:.              *
*               :$$$$$$$$: ;      ;xXXXXXXXXXXXXx: ..::::::  .::.              *
*              ;$$$$$$$$ ::   :;XXXXXXXXXXXXXXXXXX+ .::::.  .:::               *
*               X$$$$$X : +XXXXXXXXXXXXXXXXXXXXXXXX; .::  .::::.               *
*                .$$$$ :xXXXXXXXXXXXXXXXXXXXXXXXXXXX.   .:::::.                *
*                 X$$X XXXXXXXXXXXXXXXXXXXXXXXXXXXXx:  .::::.                  *
*                 $$$:.XXXXXXXXXXXXXXXXXXXXXXXXXXX  ;; ..:.                    *
*                 $$& :XXXXXXXXXXXXXXXXXXXXXXXX;  +XX; X$$;                    *
*                 $$$: XXXXXXXXXXXXXXXXXXXXXX; :XXXXX; X$$;                    *
*                 X$$X XXXXXXXXXXXXXXXXXXX; .+XXXXXXX; $$$                     *
*                 $$$$ ;XXXXXXXXXXXXXXX+  +XXXXXXXXx+ X$$$+                    *
*               x$$$$$X ;XXXXXXXXXXX+ :xXXXXXXXX+   .;$$$$$$                   *
*              +$$$$$$$$ ;XXXXXXx;;+XXXXXXXXX+    : +$$$$$$$$                  *
*               +$$$$$$$$: xXXXXXXXXXXXXXX+      ; X$$$$$$$$                   *
*                :$$$$$$$$$. +XXXXXXXXX;      ;: x$$$$$$$$$                    *
*                ;x$$$$XX$$$$+ .;+X+      :;: :$$$$$xX$$$X                     *
*               ;;;;;;;;;;X$$$$$$$+      :X$$$$$$&.                            *
*               ;;;;;;;:;;;;;x$$$$$$$$$$$$$$$$x.                               *
*               :;;;;;;;;;;;;.  :$$$$$$$$$$X                                   *
*                .;;;;;;;;:;;    +$$$$$$$$$                                    *
*                  .;;;;;;.       X$$$$$$$:                                    *
*                                                                              *
*   Unless required by applicable law or agreed to in writing, software        *
*   distributed under the License is distributed on an "AS IS" BASIS,          *
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
*   See the License for the specific language governing permissions and        *
*   limitations under the License.                                             *
*                                                                              *
*******************************************************************************/
'''

import rclpy
from rclpy.node import Node
from rclpy.time import Time as RclTime
from rclpy.qos import *

from builtin_interfaces.msg import Time as BuiltinTime
from std_msgs.msg import Float64

from csm_metrics.msg import TraceNotification, TraceNotifications, LabelStamped

class ProfilingManager(Node):
    def __init__(self):
        super().__init__('ros_profiling_manager')

        self.declare_parameter('notification_topic', 'trace_notifications')
        sub_topic = self.get_parameter(
            'notification_topic' ).get_parameter_value().string_value
        
        self.PUB_QOS = QoSProfile(
            history = QoSHistoryPolicy.KEEP_LAST,
            depth = 10,
            reliability = QoSReliabilityPolicy.RELIABLE,
            durability = QoSDurabilityPolicy.VOLATILE )

        self.notification_sub = self.create_subscription(
            TraceNotifications,
            sub_topic,
            self.notification_callback,
            self.PUB_QOS )

        self.thread_indices = {}
        self.thread_status_pubs = []
        self.thread_label_stacks = []
        self.thread_label_sets = []

        self.label_indices = {}
        self.label_dt_pubs = []
        self.label_prev_stamps = []

    def get_thread_idx(self, thread_id : int):
        if thread_id not in self.thread_indices:
            self.thread_indices[thread_id] = len(self.thread_indices)
            self.thread_status_pubs.append(list())
            self.thread_label_stacks.append(list())
            self.thread_label_sets.append(set())
        return self.thread_indices[thread_id]

    def get_label_idx(self, label : str):
        if label not in self.label_indices:
            self.label_indices[label] = len(self.label_indices)
            self.label_dt_pubs.append(
                self.create_publisher(
                    Float64,
                    f'profiling/tasks/{label}/dt',
                    qos_profile_sensor_data ) )
            self.label_prev_stamps.append(0)
        return self.label_indices[label]

    def construct_status(nanos : int, label : str):
        status = LabelStamped()
        t = RclTime(nanoseconds=nanos)
        status.stamp = t.to_msg()
        status.label = label
        return status

    def publish_status(self, thread_idx : int, depth : int, msg : LabelStamped):
        pubs = self.thread_status_pubs[thread_idx]

        while len(pubs) <= depth:
            pubs.append(
                self.create_publisher(
                    LabelStamped,
                    f'profiling/thread{thread_idx}_d{len(pubs)}',
                    self.PUB_QOS ) )

        pubs[depth].publish(msg)
        # self.get_logger().info(f'Published status change for thread {thread_idx} at depth {depth} : {msg.task}')

    def publish_dt(self, label_idx : int, value : float):
        msg = Float64()
        msg.data = value
        self.label_dt_pubs[label_idx].publish(msg)

    def notification_callback(self, msg : TraceNotifications):
        to_end = {}
        for notification in msg.notifications:
            thread_idx =  self.get_thread_idx(notification.thread_id)
            label_stack = self.thread_label_stacks[thread_idx]
            label_set = self.thread_label_sets[thread_idx]
            if not notification.label:
                # empty label is treated as a synchronization token --> clear the thread's stack, set and any queued nullifications
                label_stack.clear()
                label_set.clear()
                if thread_idx in to_end:
                    to_end[thread_idx].clear()
            else:
                label_idx = self.get_label_idx(notification.label)
                if len(label_stack) == 0 or label_stack[-1] != label_idx:
                    # stack is empty OR the label is different than the top -- this *should* be a new label
                    if(label_idx in label_set):
                        # the label was not, in fact, new
                        self.get_logger().error(
                            f'Received duplicate label {notification.label} on thread {thread_idx}! The notification will be discarded.' )
                        return
                    else:
                        # the label is new, so add it to the stack
                        label_stack.append(label_idx)
                        label_set.add(label_idx)
                        pub_depth = len(label_stack) - 1

                        # if this thread/depth previously ended, overrule by the current task
                        if thread_idx in to_end and pub_depth in to_end[thread_idx]:
                            null_t = to_end[thread_idx][pub_depth]
                            # publish "filler" if the current task doesn't immediately start after the previous
                            if null_t != notification.ns_since_epoch:
                                self.publish_status(
                                    thread_idx,
                                    pub_depth,
                                    ProfilingManager.construct_status(
                                        null_t,
                                        "" ) )
                            del to_end[thread_idx][pub_depth]

                        # store start time for this label
                        self.label_prev_stamps[label_idx] = notification.ns_since_epoch
                        # update the current thread's task at the current stack depth
                        self.publish_status(
                            thread_idx,
                            pub_depth,
                            ProfilingManager.construct_status(
                                notification.ns_since_epoch,
                                notification.label ) )
                else:
                    # the stack is not empty AND the label matches the top of the stack
                    # pop from stack and remove from set
                    label_set.remove(label_stack.pop())
                    pub_depth = len(label_stack)
                    # save this thread/depth to be nullified if no tasks in the current buffer overwrite it
                    if not thread_idx in to_end:
                        to_end[thread_idx] = {}
                    to_end[thread_idx][pub_depth] = notification.ns_since_epoch
                    # use the stored previous stamp to iterate the metrics for this task (and publish)
                    self.publish_dt(
                        label_idx,
                        float(notification.ns_since_epoch - self.label_prev_stamps[label_idx]) / 1e9 )

        # nullify any thread/depths that had a task finish but haven't been overwritten
        for thread_idx in to_end:
            depths_to_ts = to_end[thread_idx]
            for depth in depths_to_ts:
                self.publish_status(
                    thread_idx,
                    depth,
                    ProfilingManager.construct_status(
                        depths_to_ts[depth],
                        "" ) )


def main(args = None):
    rclpy.init(args = args)

    node = ProfilingManager()
    rclpy.spin(node)
    node.destroy_node()

    rclpy.shutdown()

if __name__ == '__main__':
    main()
