"""
智能助盲眼镜 综合决策引擎
=================================
: 斑马线上下文 — 有斑马线时交通灯优先，无灯时检查偏移
"""
class DecisionEngine:
    """多事件优先级仲裁"""

    def __init__(self, cfg):
        self.cfg = cfg
        self._crosswalk_announced = False  # 是否已播报过斑马线

    def evaluate(self, avg_dist, light, crosswalk, cross_offset,
                 obstacle_blob, obstacle_area, lateral_line,
                 pothole, stairs, overhead_danger, turn_advice,
                 tactile_direction='none'):
        """
        返回: (event_type, description, priority, turn_advice)

        : 斑马线上红灯优先级最高，无灯时检测偏移，
              首次进入斑马线播报"斑马线区域"。
        """

        # ============================================================
        # 斑马线上下文 — 交通灯优先，其次偏移，最后斑马线
        # ============================================================
        if crosswalk:
            # 红灯 — 无论何时都是最高优先级
            if light == 'red':
                self._crosswalk_announced = True
                return ('red', '红灯，请停下', 4, 'none')

            # 绿灯 — 可通行
            if light == 'green':
                self._crosswalk_announced = True
                return ('green', '绿灯可通行', 0, 'none')

            # 无障碍物走斑马线 — 检查偏移
            if obstacle_blob is None:
                if cross_offset is not None and cross_offset != 0:
                    if cross_offset < -20:
                        self._crosswalk_announced = True
                        return ('crosswalk_left', '偏左，请向右调整', 2, 'none')
                    elif cross_offset > 20:
                        self._crosswalk_announced = True
                        return ('crosswalk_right', '偏右，请向左调整', 2, 'none')

            # 走在斑马线中间 — 首次播报一次
            if not self._crosswalk_announced:
                self._crosswalk_announced = True
                return ('crosswalk', '斑马线区域', 1, 'none')

            # 已播报过，无事件 → 畅通
            return ('clean', '', 0, 'none')

        # 离开斑马线 — 重置状态
        self._crosswalk_announced = False

        # ============================================================
        # 非斑马线 — 常规优先级仲裁
        # ============================================================
        events = []

        # 红灯: 无斑马线时可能是车尾灯/广告牌，降级
        if light == 'red':
            events.append((2, 'red', '疑似红灯，请确认'))

        if overhead_danger and avg_dist < self.cfg.OVERHEAD_DIST_THRESH:
            events.append((4, 'overhead', '头顶障碍物，请低头'))
        if pothole in ('pothole', 'bump'):
            desc = '危险！前方坑洞' if pothole == 'pothole' else '小心路面凸起'
            events.append((3, pothole, desc))
        if (obstacle_blob is not None
                and obstacle_area > self.cfg.AREA_BLOCK
                and avg_dist < self.cfg.DIST_BLOCK):
            events.append((3, 'obstacle', '大型障碍物阻挡'))
        elif (obstacle_blob is not None
                and obstacle_area > self.cfg.AREA_CAUTION
                and avg_dist < self.cfg.DIST_CAUTION):
            events.append((2, 'obstacle_near', '前方有障碍物%.0f厘米' % avg_dist))
        if lateral_line and avg_dist < self.cfg.LATERAL_DIST_THRESH:
            events.append((3, 'lateral', '横向拦截物，请绕行'))
        if tactile_direction in ('left', 'right'):
            guidance = '偏左，请向右调整' if tactile_direction == 'left' else '偏右，请向左调整'
            events.append((2, 'tactile_warn', guidance))
        if stairs == 'up':
            events.append((1, 'stairs_up', '前方上楼梯'))
        elif stairs == 'down':
            events.append((3, 'stairs_down', '危险！前方下楼梯'))
        elif stairs == 'potential':
            events.append((1, 'stairs_up', '前方疑似楼梯'))
        if turn_advice in ('left', 'right', 'stop') and not events:
            direction_word = '左' if turn_advice == 'left' else '右'
            events.append((0, turn_advice, '请向%s绕行' % direction_word))

        if not events:
            return ('clean', '前方畅通', 0, 'none')

        event_order = {'obstacle': 0, 'lateral': 1, 'pothole': 2, 'bump': 2,
                       'obstacle_near': 5, 'tactile_warn': 6,
                       'stairs_down': 10,
                       'crosswalk': 12, 'stairs_up': 13, 'green': 14,
                       'left': 15, 'right': 16}
        events.sort(key=lambda x: (-x[0], event_order.get(x[1], 99)))
        top = events[0]
        return (top[1], top[2], top[0], turn_advice)