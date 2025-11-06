# Mila Monorepo
Mila monorepo is PROVE's monorepo for all code related to the Mila
vehicle (except for embedded code, for now). It includes:
- Dashboard and Reverse camera
- Homebase telem code
- SerDes and Plotting scripts for logged data
- Documentation and a single source of truth
- etc

Essentially all code that is necessary for Mila will eventually end up in this repository.

## Structure
```
/
    /.github
    /tools
        /plot
    /dashboard
    /reverse-camera
    /telem
    /doc
        /memos
        /rfc
        /test_result/
    /experiments
        /bms
```

Future:
```
    /embedded
        /mcu
        /vitals
        /imu... etc
    /autonomy
```

## Documentation
All new PROVE Memos will now reside in /doc as Markdown for CS and CPE-related
items. This keeps the documentation close to the code, beneficial both for reviewers
and devs

