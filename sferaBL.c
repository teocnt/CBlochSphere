#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>


#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 900
#define OFFSETX WINDOW_WIDTH/2
#define OFFSETY WINDOW_HEIGHT/2
#define SCALE 400
#define DELAY 20
#define PRESPECTIVE 3.0

#define RADIUS 1
#define NPAR 12
#define NMER 24



typedef int Index;

typedef struct{
  double theta;
  double phi;
}Polar2;

typedef struct{
  double x;
  double y;
}Vec2;

typedef struct{
  double x;
  double y;
  double z;
}Vec3;

typedef struct{
  Index start;
  Index end;
}Connection;

typedef struct{
  double radius;
  Vec3 *points;
  int nPoints;
  Connection *connections;
  int nConnections;
}Sphere;

// parametri per motore grafico
double globalScaling = SCALE;
double xOffset = OFFSETX;
double yOffset = OFFSETY;


//oggetti SDL
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event event;



Vec3 fromPolarToCartesian(Polar2);
void calculateSphereModel(Sphere*);
void calculateSpherePoints(int, Vec3*);
void calculateSphereConnections(int, Connection*);
void drawSphere(Sphere*,bool);
Vec2 projectsP3(Vec3);
void drawSystemVector(Polar2);
void drawThickLine(SDL_Renderer*,Vec2,Vec2,int);


int main(void){
  bool execution = true;
  Polar2 systemState = {0,0};
  //modello sfera
  Sphere sphereData = {1,NULL,NPAR*NMER,NULL,(NPAR*NMER) + ((NPAR-1)*NMER)};
  sphereData.points = malloc(sphereData.nPoints*sizeof(Vec3));
  sphereData.connections = malloc(sphereData.nConnections*sizeof(Connection));
  calculateSphereModel(&sphereData); 

  if(SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("errore durante l'inizializzazione di SDL: %s\n",SDL_GetError());
    return 1;
  }

  window = SDL_CreateWindow(
    "rotating cube",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    0
  );


  if(window == NULL){
    printf("errore nella creazione della finestra: %s\n",SDL_GetError());
    SDL_Quit();
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  if (renderer == NULL) {
    printf("Errore nella creazione del renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  while(execution){

    //event handling
    while(SDL_PollEvent(&event)){
      if(event.type == SDL_QUIT){
        execution = false;
      }else if(event.type == SDL_KEYDOWN){
        if(event.key.keysym.sym == SDLK_ESCAPE){
          execution = false;
        }
      }


    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0,0, 255);
    SDL_RenderClear(renderer);
    
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
    drawSphere(&sphereData,false);
    

    SDL_SetRenderDrawColor(renderer, 0,0,255,255);
    drawSystemVector(systemState);
    
    SDL_SetRenderDrawColor(renderer, 255,255,255,125);
    drawSphere(&sphereData,true);
    

    SDL_RenderPresent(renderer);



  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();

  return EXIT_SUCCESS;

}


Vec3 fromPolarToCartesian(Polar2 polarCoordinates){
  Vec3 cartesianCoordinates;

  cartesianCoordinates.y = cos(polarCoordinates.theta);
  cartesianCoordinates.x = sin(polarCoordinates.theta) * cos(polarCoordinates.phi);
  cartesianCoordinates.z = sin(polarCoordinates.theta) * sin(polarCoordinates.phi);
  return cartesianCoordinates;

}

void calculateSphereModel(Sphere *sphere){
  calculateSpherePoints(sphere->nPoints,sphere->points);
  calculateSphereConnections(sphere->nConnections, sphere->connections);
}

void calculateSpherePoints(int nPoints, Vec3* points){
  Polar2 polarCoordinates = {0,0};
  double passoTheta = M_PI/(NPAR - 1);
  double passoPhi = (2 * M_PI)/(NMER);
  Index pointIndex = 0;
  for(int i = 0; i < NPAR; i++){
    polarCoordinates.theta = passoTheta*i;
    for(int j = 0; j < NMER; j++){
      polarCoordinates.phi = passoPhi * j;
      points[pointIndex] = fromPolarToCartesian(polarCoordinates);
      pointIndex++;
    }
  }
}


void calculateSphereConnections(int nConnections,Connection *connections){
  Index connectionIndex = 0;
  Index currentPoint;
  Index nextPoint;
  for(int i = 0; i < NPAR; i++){
    for(int j = 0; j < NMER; j++){
      currentPoint = (i*NMER) + j;

      nextPoint = (i * NMER) + ((j+1)%NMER);
      connections[connectionIndex].start = currentPoint;
      connections[connectionIndex].end = nextPoint;
      connectionIndex++;


      if (i < NPAR -1){
        nextPoint = ((i+1)*NMER)+j;
        connections[connectionIndex].start = currentPoint;
        connections[connectionIndex].end = nextPoint;
        connectionIndex++;
      }
    }
  }
}


void drawSphere(Sphere *sphere,bool drawFront){
  Vec2 projectedPoints[sphere->nPoints];
  Connection tmpC;
  Vec2 p1;
  Vec2 p2;
  for(int i = 0;i < sphere->nPoints; i++){
    projectedPoints[i] = projectsP3(sphere->points[i]);
  }
  for(int i = 0; i < sphere->nConnections; i++){
    tmpC = sphere->connections[i];
    p1 = projectedPoints[tmpC.start];
    p2 =  projectedPoints[tmpC.end];


    double z1 = sphere->points[tmpC.start].z;
    double z2 = sphere->points[tmpC.end].z;
    double zMid = (z1 + z2) / 2.0;

    if(drawFront && zMid > 0) continue;
    
    if(!drawFront && zMid <= 0) continue;
    
    SDL_RenderDrawLine(renderer,p1.x,p1.y,p2.x,p2.y);
  }
}


Vec2 projectsP3(Vec3 point){
  double x2D = point.x/(point.z+PRESPECTIVE);
  double y2D = point.y/(point.z+PRESPECTIVE);
  x2D = round((x2D*globalScaling)+xOffset);
  y2D = round(yOffset - (y2D * globalScaling));
  Vec2 pointRet = {x2D,y2D};
  return pointRet;
}

void drawSystemVector(Polar2 systemState){
  Vec3 cartesianPoint = fromPolarToCartesian(systemState);
  Vec2 pPoint = projectsP3(cartesianPoint);
  Vec2 origin = {xOffset,yOffset};
  SDL_Rect systemRect = {
    .x = pPoint.x-1,
    .y = pPoint.y-1,
    .w = 3,
    .h = 3
  };
  SDL_RenderFillRect(renderer, &systemRect);
  drawThickLine(renderer,origin,pPoint,3);

}

void drawThickLine(SDL_Renderer *renderer,Vec2 p1, Vec2 p2, int thickness){
  if(thickness == 0){
    return;
  }
  
  double dx = fabs(p2.x - p1.x);
  double dy = fabs(p2.y - p1.y);

  if(dx > dy){
    SDL_RenderDrawLine(renderer, p1.x,p1.y+thickness-1,p2.x,p2.y+thickness-1);
    SDL_RenderDrawLine(renderer, p1.x,p1.y-(thickness-1),p2.x,p2.y-(thickness-1));
  }else{
    SDL_RenderDrawLine(renderer, p1.x+thickness-1,p1.y,p2.x+thickness-1,p2.y);
    SDL_RenderDrawLine(renderer, p1.x-(thickness-1),p1.y,p2.x-(thickness-1),p2.y);
  }


  drawThickLine(renderer,p1,p2,thickness-1);
}
