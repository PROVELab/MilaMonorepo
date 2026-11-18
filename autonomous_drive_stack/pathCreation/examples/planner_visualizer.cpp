// Standard library only. Runs the real planner and writes an offline HTML viewer.
#include "scenarios.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>
#include <utility>

using namespace prove::planning;
namespace fixtures = prove::planning::scenarios;

namespace {
struct Scene {
    std::string name, description;
    OccupancyGrid grid;
    Pose start = fixtures::start, goal = fixtures::goal;
    Config config;
    bool planning = true;
    Polygon boundary;
    std::vector<CircleObstacle> circles;
    std::vector<Polygon> polygons;
    std::vector<BoxObstacle> boxes;
};

OccupancyGrid checked(GridBuildResult map) {
    if (!map.success) throw std::runtime_error(map.reason);
    return std::move(map.grid);
}

std::vector<Scene> scenes() {
    std::vector<Scene> result;
    Scene s;
    s.name = "Planner / open route";
    s.description = "Open grid from planner_test.cpp. The rear axle should travel straight toward the goal.";
    s.grid = fixtures::openGrid();
    result.push_back(s);
    s.name = "Planner / obstacle detour";
    s.description = "The test's 1 m square obstacle requires clearance for the whole vehicle, including its safety margin.";
    fixtures::addDetourObstacle(s.grid);
    result.push_back(s);
    s.name = "Planner / detour without smoothing";
    s.description = "Same obstacle and search, with smooth_path disabled. Compare steering and the route with the previous scene.";
    s.config.smooth_path = false;
    result.push_back(s);
    s.config.smooth_path = true;
    s.name = "Planner / unknown wall";
    s.description = "Unknown cells block the entire map height. The previous detour must be discarded on failure.";
    fixtures::addUnknownWall(s.grid);
    result.push_back(s);
    s.grid = fixtures::openGrid();
    fixtures::addFrontObstacle(s.grid);
    s.name = "Planner / obstacle under front";
    s.description = "The rear axle is free, but the vehicle's front overlaps an obstacle at the start.";
    result.push_back(s);
    s.grid = fixtures::openGrid();
    s.start = {s.grid.origin_x + 0.1, 0, 0};
    s.name = "Planner / body outside map";
    s.description = "The axle is inside the grid, but the rear of the vehicle extends beyond the known map.";
    result.push_back(s);
    s.start = fixtures::start;
    s.name = "Planner / restored open map";
    s.description = "Fresh open observation: the route can be used again.";
    result.push_back(s);
    s.config.require_goal_yaw = true;
    s.name = "Planner / goal heading";
    s.description = "Open map with goal heading required, as in the heading-constrained test.";
    result.push_back(s);

    s = Scene{};
    s.start = fixtures::road_start;
    s.goal = fixtures::road_goal;
    s.boundary = fixtures::road;
    s.grid = checked(buildOccupancyGrid(fixtures::road_geometry, s.boundary));
    s.name = "Grid builder / open road";
    s.description = "Geometry-to-grid-to-planner handoff from grid_builder_test.cpp. Cells touching the road boundary are blocked.";
    result.push_back(s);
    s.circles = fixtures::road_circles;
    s.grid = checked(buildOccupancyGrid(fixtures::road_geometry, s.boundary, s.circles));
    s.name = "Grid builder / circle detour";
    s.description = "Exact test circle: center (15, 10), radius 0.75 m. Orange outlines are input geometry; dark cells show rasterization.";
    result.push_back(s);
    s.circles.clear();
    s.polygons = {fixtures::wall};
    s.config.max_expansions = 15000;
    s.grid = checked(buildOccupancyGrid(fixtures::road_geometry, s.boundary, {}, s.polygons));
    s.name = "Grid builder / blocking wall";
    s.description = "The test wall spans the road. An unsuccessful result contains no playable path.";
    result.push_back(s);

    s = Scene{};
    s.planning = false;
    s.boundary = fixtures::area;
    s.circles = fixtures::circles;
    s.polygons = {fixtures::obstacle};
    s.grid = checked(buildOccupancyGrid(fixtures::geometry, s.boundary, s.circles, s.polygons));
    s.name = "Rasterization / circle and polygon";
    s.description = "Grid-only test: cells touched by the circle or tiny polygon are occupied, even if the cell center is outside the shape.";
    result.push_back(s);
    s.circles.clear();
    s.polygons.clear();
    s.boxes = {fixtures::box};
    s.grid = checked(buildOccupancyGrid(fixtures::geometry, s.boundary, {}, {}, s.boxes));
    s.name = "Rasterization / rotated box";
    s.description = "Grid-only test: the 2 m by 0.3 m box is rotated 90 degrees. Compare physical shape with occupied cells.";
    result.push_back(s);
    s.boxes.clear();
    const auto corridor = closeCorridor(fixtures::left, fixtures::right);
    if (!corridor.success) throw std::runtime_error(corridor.reason);
    s.boundary = corridor.polygon;
    s.grid = checked(buildOccupancyGrid(fixtures::geometry, s.boundary));
    s.name = "Rasterization / lane corridor";
    s.description = "Grid-only test: ordered left and right boundaries are joined by straight end caps. Outside is unknown.";
    result.push_back(s);
    for (const auto& fixture : fixtures::staticScenarios()) {
        s = Scene{};
        s.name = fixture.name;
        s.description = fixture.description;
        s.start = fixture.start;
        s.goal = fixture.goal;
        s.config = fixture.config;
        s.boundary = fixture.boundary;
        if (!fixture.left.empty()) {
            const auto lane = closeCorridor(fixture.left, fixture.right);
            if (!lane.success) throw std::runtime_error(lane.reason);
            s.boundary = lane.polygon;
        }
        s.circles = fixture.circles;
        s.polygons = fixture.polygons;
        s.boxes = fixture.boxes;
        s.grid = checked(buildOccupancyGrid(fixture.geometry, s.boundary,
                                           s.circles, s.polygons, s.boxes));
        result.push_back(std::move(s));
    }
    return result;
}

void quoted(std::ostream& out, const std::string& value) {
    out << '"';
    for (unsigned char c : value) {
        // Escape '<' too, so strings cannot close the HTML script element.
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32 || c == '<' || c >= 127)
            out << "\\u00" << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
        else out << c;
    }
    out << '"';
}
void pose(std::ostream& out, Pose p) { out << '[' << p.x << ',' << p.y << ',' << p.yaw << ']'; }
void polygon(std::ostream& out, const Polygon& points) {
    out << '[';
    bool first = true;
    for (const auto& p : points) {
        if (!first) out << ',';
        first = false;
        out << '[' << p.x << ',' << p.y << ']';
    }
    out << ']';
}
void writeScene(std::ostream& out, const Scene& s, const Result& r) {
    out << "{\"name\":"; quoted(out, s.name);
    out << ",\"description\":"; quoted(out, s.description);
    out << ",\"planning\":" << (s.planning ? "true" : "false")
        << ",\"success\":" << (r.success ? "true" : "false") << ",\"reason\":";
    quoted(out, r.reason);
    out << ",\"expanded\":" << r.expanded << ",\"cost\":" << r.cost
        << ",\"smoothed\":" << (r.smoothing_applied ? "true" : "false")
        << ",\"grid\":{\"width\":" << s.grid.width << ",\"height\":" << s.grid.height
        << ",\"resolution\":" << s.grid.resolution << ",\"x\":" << s.grid.origin_x
        << ",\"y\":" << s.grid.origin_y << ",\"cells\":[";
    for (std::size_t i = 0; i < s.grid.cells.size(); ++i) {
        if (i) out << ',';
        out << static_cast<int>(s.grid.cells[i]);
    }
    out << "]},\"start\":"; pose(out, s.start);
    out << ",\"goal\":"; pose(out, s.goal);
    const auto& c = s.config;
    out << ",\"config\":{\"front\":" << c.front_extent << ",\"rear\":" << c.rear_extent
        << ",\"halfWidth\":" << c.half_width << ",\"margin\":" << c.safety_margin
        << ",\"wheelbase\":" << c.wheelbase << ",\"threshold\":" << c.occupied_threshold
        << ",\"tolerance\":" << c.goal_tolerance << "},\"boundary\":";
    polygon(out, s.boundary);
    out << ",\"polygons\":[";
    for (std::size_t i = 0; i < s.polygons.size(); ++i) {
        if (i) out << ',';
        polygon(out, s.polygons[i]);
    }
    out << "],\"circles\":[";
    for (std::size_t i = 0; i < s.circles.size(); ++i) {
        if (i) out << ',';
        const auto& p = s.circles[i];
        out << '[' << p.center.x << ',' << p.center.y << ',' << p.radius << ']';
    }
    out << "],\"boxes\":[";
    for (std::size_t i = 0; i < s.boxes.size(); ++i) {
        if (i) out << ',';
        const auto& b = s.boxes[i];
        out << '[' << b.center.x << ',' << b.center.y << ',' << b.length << ',' << b.width << ',' << b.yaw << ']';
    }
    out << "],\"path\":[";
    for (std::size_t i = 0; i < r.path.size(); ++i) {
        if (i) out << ',';
        const auto& p = r.path[i];
        out << '[' << p.pose.x << ',' << p.pose.y << ',' << p.pose.yaw << ',' << p.steering << ',' << p.curvature << ']';
    }
    out << "]}";
}

const char* viewer = R"HTML(
];
const $=id=>document.getElementById(id), canvas=$('map'), ctx=canvas.getContext('2d');
let current=0, sample=0, playing=false, lastTick=0;
for (const [i,s] of scenes.entries()) { const o=document.createElement('option'); o.value=i; o.textContent=s.name; $('scene').append(o); }
function stop(){playing=false;$('play').textContent='Play';}
function select(index){
  stop();current=(index+scenes.length)%scenes.length;sample=0;const s=scenes[current];
  $('scene').value=current;$('description').textContent=s.description;
  $('status').textContent=s.planning?(s.success?'Path found':'No path: '+s.reason):'Grid build only';
  $('status').style.color=!s.planning?'#334155':s.success?'#047857':'#b91c1c';
  $('stats').textContent=s.planning?`${s.path.length} samples | ${s.expanded} expanded states | search cost ${s.cost.toFixed(2)} | smoothing ${s.smoothed?'applied':'not applied'}`:'Input geometry and resulting occupancy cells; no planning requested.';
  $('scrub').max=Math.max(0,s.path.length-1);$('scrub').value=0;
  $('play').disabled=$('scrub').disabled=s.path.length<2;
  draw();
}
function draw(){
  const s=scenes[current],g=s.grid,c=s.config,box=canvas.getBoundingClientRect(),dpr=window.devicePixelRatio||1;
  canvas.width=Math.round(box.width*dpr);canvas.height=Math.round(box.height*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);
  const w=box.width,h=box.height,pad=45,scale=Math.min((w-2*pad)/(g.width*g.resolution),(h-2*pad)/(g.height*g.resolution));
  const ox=(w-g.width*g.resolution*scale)/2,oy=(h+g.height*g.resolution*scale)/2;
  const X=x=>ox+(x-g.x)*scale,Y=y=>oy-(y-g.y)*scale;
  ctx.clearRect(0,0,w,h);ctx.fillStyle='#f8fafc';ctx.fillRect(0,0,w,h);
  for(let row=0;row<g.height;row++)for(let col=0;col<g.width;col++){
    const v=g.cells[row*g.width+col];ctx.fillStyle=v<0?'#b8c4d4':v>=c.threshold?'#334155':'#ffffff';
    ctx.fillRect(X(g.x+col*g.resolution),Y(g.y+(row+1)*g.resolution),g.resolution*scale+.3,g.resolution*scale+.3);
  }
  ctx.lineWidth=.5;ctx.strokeStyle='#dbe2ea';
  if($('grid').checked){
    ctx.beginPath();for(let col=0;col<=g.width;col++){const x=X(g.x+col*g.resolution);ctx.moveTo(x,Y(g.y));ctx.lineTo(x,Y(g.y+g.height*g.resolution));}
    for(let row=0;row<=g.height;row++){const y=Y(g.y+row*g.resolution);ctx.moveTo(X(g.x),y);ctx.lineTo(X(g.x+g.width*g.resolution),y);}ctx.stroke();
  }
  function line(points,color,width=2,closed=false){if(!points.length)return;ctx.beginPath();points.forEach((p,i)=>i?ctx.lineTo(X(p[0]),Y(p[1])):ctx.moveTo(X(p[0]),Y(p[1])));if(closed)ctx.closePath();ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();}
  function rectangle(x,y,front,rear,half,yaw){const co=Math.cos(yaw),si=Math.sin(yaw);return [[front,half],[front,-half],[-rear,-half],[-rear,half]].map(([a,b])=>[x+a*co-b*si,y+a*si+b*co]);}
  if($('geometry').checked){
    line(s.boundary,'#d97706');s.polygons.forEach(p=>line(p,'#d97706'));
    for(const [x,y,r] of s.circles){ctx.beginPath();ctx.arc(X(x),Y(y),r*scale,0,2*Math.PI);ctx.strokeStyle='#d97706';ctx.lineWidth=2;ctx.stroke();}
    for(const [x,y,l,b,yaw] of s.boxes)line(rectangle(x,y,l/2,l/2,b/2,yaw),'#d97706',2,true);
  }
  function body(p,alpha=1){
    ctx.save();ctx.globalAlpha=alpha;
    if($('margin').checked){ctx.setLineDash([5,4]);line(rectangle(p[0],p[1],c.front+c.margin,c.rear+c.margin,c.halfWidth+c.margin,p[2]),'#7c3aed',1.5,true);ctx.setLineDash([]);}
    const corners=rectangle(p[0],p[1],c.front,c.rear,c.halfWidth,p[2]);
    line(corners,'#0369a1',2,true);ctx.fillStyle='#0ea5e930';ctx.fill();
    line([[p[0],p[1]],[p[0]+c.front*Math.cos(p[2]),p[1]+c.front*Math.sin(p[2])]],'#0369a1',2);
    if(p.length>3){const x=p[0]+c.wheelbase*Math.cos(p[2]),y=p[1]+c.wheelbase*Math.sin(p[2]),a=p[2]+p[3];line([[x-.35*Math.cos(a),y-.35*Math.sin(a)],[x+.35*Math.cos(a),y+.35*Math.sin(a)]],'#0f172a',3);}
    ctx.beginPath();ctx.arc(X(p[0]),Y(p[1]),3,0,2*Math.PI);ctx.fillStyle='#0369a1';ctx.fill();ctx.restore();
  }
  if(s.planning){
    ctx.beginPath();ctx.arc(X(s.goal[0]),Y(s.goal[1]),c.tolerance*scale,0,2*Math.PI);ctx.fillStyle='#10b98118';ctx.fill();ctx.strokeStyle='#059669';ctx.lineWidth=1;ctx.stroke();
    line(s.path,'#059669',3);
    if($('footprints').checked)for(let i=0;i<s.path.length;i+=Math.max(1,Math.ceil(s.path.length/18)))body(s.path[i],.2);
    for(const [p,label,color] of [[s.start,'Start','#0369a1'],[s.goal,'Goal','#047857']]){
      ctx.beginPath();ctx.arc(X(p[0]),Y(p[1]),4,0,2*Math.PI);ctx.fillStyle=color;ctx.fill();ctx.font='bold 12px system-ui';ctx.fillText(label,X(p[0])+7,Y(p[1])-8);
      line([[p[0],p[1]],[p[0]+Math.cos(p[2]),p[1]+Math.sin(p[2])]],color);
    }
    const p=s.path[sample]||s.start;body(p);
    $('sample').textContent=s.path.length?`Sample ${sample+1} / ${s.path.length} | rear axle (${p[0].toFixed(2)}, ${p[1].toFixed(2)}) m | yaw ${(p[2]*180/Math.PI).toFixed(1)} deg | steering ${(p[3]*180/Math.PI).toFixed(1)} deg | curvature ${p[4].toFixed(3)} /m`:'No path samples. Showing the starting footprint.';
  }else $('sample').textContent='Grid resolution: '+g.resolution+' m per cell';
  ctx.fillStyle='#475569';ctx.font='11px system-ui';ctx.textAlign='center';
  const step=Math.max(1,Math.ceil(g.width*g.resolution/10));
  for(let x=Math.ceil(g.x/step)*step;x<=g.x+g.width*g.resolution;x+=step)ctx.fillText(x+' m',X(x),oy+20);
  ctx.textAlign='right';for(let y=Math.ceil(g.y/step)*step;y<=g.y+g.height*g.resolution;y+=step)ctx.fillText(y+' m',ox-7,Y(y)+4);
}
$('scene').onchange=e=>select(Number(e.target.value));$('prev').onclick=()=>select(current-1);$('next').onclick=()=>select(current+1);
$('play').onclick=()=>{if(playing)stop();else{if(sample>=scenes[current].path.length-1)sample=0;playing=true;lastTick=performance.now();$('play').textContent='Pause';}};
$('scrub').oninput=e=>{stop();sample=Number(e.target.value);draw();};
for(const id of ['grid','geometry','margin','footprints'])$(id).onchange=draw;
window.addEventListener('resize',draw);
function tick(time){if(playing&&time-lastTick>=100){lastTick=time;sample++;if(sample>=scenes[current].path.length-1){sample=scenes[current].path.length-1;stop();}$('scrub').value=sample;draw();}requestAnimationFrame(tick);}
select(0);requestAnimationFrame(tick);
</script></body></html>
)HTML";
} // namespace

int main(int argc, char** argv) {
    if (argc > 2) { std::cerr << "Usage: planner_visualizer [output.html]\n"; return 1; }
    const std::string output = argc == 2 ? argv[1] : "planner_visualizer.html";
    try {
        std::ofstream out(output);
        if (!out) throw std::runtime_error("Cannot open output: " + output);
        out.imbue(std::locale::classic());
        out << std::setprecision(17);
        out << R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Planner test visualizer</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#edf2f7;color:#172b40;font:14px system-ui,sans-serif}main{max-width:1200px;margin:24px auto;padding:0 16px}h1{font-size:25px;margin:0 0 6px}p{line-height:1.5;margin:8px 0}section{background:white;border:1px solid #dbe2ea;border-radius:10px;padding:16px;margin-top:14px}.controls{display:flex;align-items:center;gap:10px;flex-wrap:wrap}button,select{font:inherit;padding:8px 12px;border:1px solid #b8c4d4;border-radius:5px;background:white;color:#172b40}button{cursor:pointer}button:disabled{opacity:.45;cursor:default}select{flex:1;min-width:220px}#map{display:block;width:100%;height:clamp(310px,55vw,570px)}#scrub{flex:1;min-width:120px}#status{font-weight:700}#stats,#sample,.note{font-size:12px;color:#475569}.legend{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px}.swatch{display:inline-block;width:12px;height:12px;vertical-align:-1px;margin-right:5px}.options{margin:12px 0}label{cursor:pointer}.note{margin-bottom:0}
</style></head><body><main><h1>Planner test visualizer</h1>
<p>Real C++ planner results from the test scenes. Coordinates in meters; +y points up.</p>
<section><div class="controls"><button id="prev" aria-label="Previous scene">Previous</button><select id="scene" aria-label="Test scene"></select><button id="next">Next</button></div>
<p id="description"></p><p id="status" role="status"></p><p id="stats"></p></section>
<section><div class="controls options"><label><input type="checkbox" id="grid" checked> Cell grid</label><label><input type="checkbox" id="geometry" checked> Input geometry</label><label><input type="checkbox" id="margin" checked> Safety margin</label><label><input type="checkbox" id="footprints"> Footprints along path</label></div>
<canvas id="map" aria-label="Top-down occupancy map, planned path, and vehicle footprint"></canvas>
<div class="legend"><span><i class="swatch" style="background:#334155"></i>Occupied</span><span><i class="swatch" style="background:#b8c4d4"></i>Unknown / blocked</span><span><i class="swatch" style="background:#d97706"></i>Input geometry</span><span><i class="swatch" style="background:#059669"></i>Rear-axle path / goal</span><span><i class="swatch" style="background:#0369a1"></i>Vehicle</span><span><i class="swatch" style="background:#7c3aed"></i>Safety margin</span></div></section>
<section><div class="controls"><button id="play">Play</button><input id="scrub" type="range" min="0" max="0" value="0" aria-label="Path sample"></div><p id="sample"></p>
<p class="note">Playback steps through returned path samples; it is not a speed simulation or a replay of search expansions. Grid-only scenes show rasterization. Invalid-input validation tests have no spatial scene.</p></section>
</main><script>
const scenes = [
)HTML";
        bool first = true;
        for (const auto& scene : scenes()) {
            std::cout << "Planning " << scene.name << "..." << std::endl;
            const Result result = scene.planning ? plan(scene.grid, scene.start, scene.goal, scene.config) : Result{};
            if (!first) out << ',';
            first = false;
            writeScene(out, scene, result);
        }
        out << viewer;
        out.close();
        if (!out) throw std::runtime_error("Could not finish writing: " + output);
        std::cout << "Wrote " << output << ". Open it in your browser.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
